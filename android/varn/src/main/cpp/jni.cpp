#include "varn/dataview/DataViewJni.h"
#include "varn/http/AndroidHttpBridge.h"
#include "varn/log/Log.h"
#include "varn/varn.h"

#include <jni.h>

#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{

// The classes and members of Java the bindings reach, resolved once in `JNI_OnLoad` while the class loader of the app is the one in reach.
class JavaSymbols
{
public:
    JavaVM* vm = nullptr;
    jclass text = nullptr;
    jclass invalidArgument = nullptr;
    jobject utf8 = nullptr;
    jmethodID textFromBytes = nullptr;
    jmethodID textBytes = nullptr;
    jmethodID hostCall = nullptr;
    jmethodID consoleLine = nullptr;
    jmethodID wakeRun = nullptr;
    jfieldID pollRan = nullptr;
    jfieldID pollPending = nullptr;
    jfieldID pollIdle = nullptr;

    static JavaSymbols& instance()
    {
        static JavaSymbols symbols;
        return symbols;
    }
};

// Resolves what the bindings reach and remembers the first class or member that is missing, which R8 stripping or renaming it is the usual cause of.
class JavaResolver
{
public:
    explicit JavaResolver(JNIEnv* env)
        : environment(env)
    {
    }

    jclass pinClass(const char* name)
    {
        jclass found = findClass(name);
        if (found == nullptr)
        {
            return nullptr;
        }

        auto pinned = static_cast<jclass>(environment->NewGlobalRef(found));
        environment->DeleteLocalRef(found);
        return pinned;
    }

    jobject pinStaticObject(const char* className, const char* name, const char* signature)
    {
        jclass type = findClass(className);
        if (type == nullptr)
        {
            return nullptr;
        }

        jobject pinned = nullptr;
        jfieldID field = environment->GetStaticFieldID(type, name, signature);
        if (field == nullptr)
        {
            fail(className, name);
        }
        else
        {
            jobject value = environment->GetStaticObjectField(type, field);
            pinned = environment->NewGlobalRef(value);
            environment->DeleteLocalRef(value);
        }

        environment->DeleteLocalRef(type);
        return pinned;
    }

    jmethodID method(const char* className, const char* name, const char* signature)
    {
        jclass type = findClass(className);
        if (type == nullptr)
        {
            return nullptr;
        }

        jmethodID found = environment->GetMethodID(type, name, signature);
        environment->DeleteLocalRef(type);
        if (found == nullptr)
        {
            fail(className, name);
        }

        return found;
    }

    jfieldID field(const char* className, const char* name, const char* signature)
    {
        jclass type = findClass(className);
        if (type == nullptr)
        {
            return nullptr;
        }

        jfieldID found = environment->GetFieldID(type, name, signature);
        environment->DeleteLocalRef(type);
        if (found == nullptr)
        {
            fail(className, name);
        }

        return found;
    }

    const std::string& getMissing() const
    {
        return missing;
    }

private:
    JNIEnv* environment = nullptr;
    std::string missing;

    jclass findClass(const char* name)
    {
        jclass found = environment->FindClass(name);
        if (found == nullptr)
        {
            fail(name, nullptr);
        }

        return found;
    }

    void fail(const char* className, const char* member)
    {
        environment->ExceptionClear();
        if (!missing.empty())
        {
            return;
        }

        missing = member != nullptr ? std::string("the member \"") + member + "\" of the Java class \"" + className + "\"" : std::string("the Java class \"") + className + "\"";
    }
};

// A call into Java can come from a thread Java has never seen, such as a pool thread that writes a line, which is attached only while it calls.
class Attachment
{
public:
    Attachment()
    {
        JavaVM* machine = JavaSymbols::instance().vm;
        if (machine->GetEnv(reinterpret_cast<void**>(&environment), JNI_VERSION_1_6) == JNI_OK)
        {
            return;
        }

        if (machine->AttachCurrentThreadAsDaemon(&environment, nullptr) != JNI_OK)
        {
            environment = nullptr;
            return;
        }

        attached = true;
    }

    ~Attachment()
    {
        if (attached)
        {
            JavaSymbols::instance().vm->DetachCurrentThread();
        }
    }

    Attachment(const Attachment&) = delete;
    Attachment& operator=(const Attachment&) = delete;

    JNIEnv* env() const
    {
        return environment;
    }

private:
    JNIEnv* environment = nullptr;
    bool attached = false;
};

// The thread that runs Lua may never return to Java, so every call into Java gives its local references back before it returns.
class LocalFrame
{
public:
    LocalFrame(JNIEnv* env, jint capacity)
        : environment(env)
        , pushed(env->PushLocalFrame(capacity) == JNI_OK)
    {
    }

    ~LocalFrame()
    {
        if (pushed)
        {
            environment->PopLocalFrame(nullptr);
        }
    }

    LocalFrame(const LocalFrame&) = delete;
    LocalFrame& operator=(const LocalFrame&) = delete;

    bool isPushed() const
    {
        return pushed;
    }

private:
    JNIEnv* environment = nullptr;
    bool pushed = false;
};

// A callback of Java that native code keeps, whose global reference is given back on whichever thread drops it.
class JavaCallback
{
public:
    JavaCallback(JNIEnv* env, jobject callback)
        : reference(env->NewGlobalRef(callback))
    {
    }

    ~JavaCallback()
    {
        Attachment attachment;
        if (attachment.env() != nullptr)
        {
            attachment.env()->DeleteGlobalRef(reference);
        }
    }

    JavaCallback(const JavaCallback&) = delete;
    JavaCallback& operator=(const JavaCallback&) = delete;

    jobject get() const
    {
        return reference;
    }

private:
    jobject reference = nullptr;
};

class JavaFailure
{
public:
    // An exception left pending would break the next call into Java, so it is printed to logcat and cleared.
    static bool clear(JNIEnv* env)
    {
        if (env->ExceptionCheck() != JNI_TRUE)
        {
            return false;
        }

        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }

    static void throwArgument(JNIEnv* env, const char* message)
    {
        env->ThrowNew(JavaSymbols::instance().invalidArgument, message);
    }
};

// Text crosses as the bytes of UTF-8 rather than the modified UTF-8 of JNI, so a character outside the basic plane keeps its four bytes both ways.
class JavaText
{
public:
    // Answers nothing with a Java exception pending when the text cannot cross, since the C API ends a text at its first null byte.
    static std::optional<std::string> toUtf8(JNIEnv* env, jstring value)
    {
        const JavaSymbols& symbols = JavaSymbols::instance();
        auto bytes = static_cast<jbyteArray>(env->CallObjectMethod(value, symbols.textBytes, symbols.utf8));
        if (bytes == nullptr)
        {
            return std::nullopt;
        }

        const jsize size = env->GetArrayLength(bytes);
        std::string text(static_cast<std::size_t>(size), '\0');
        env->GetByteArrayRegion(bytes, 0, size, reinterpret_cast<jbyte*>(text.data()));
        env->DeleteLocalRef(bytes);

        if (text.find('\0') != std::string::npos)
        {
            JavaFailure::throwArgument(env, "The text holds the character U+0000, which the runtime cannot receive.");
            return std::nullopt;
        }

        return text;
    }

    // Answers null with a Java exception pending when the string cannot be made, and bytes that are not UTF-8 become the replacement character.
    static jstring toJava(JNIEnv* env, std::string_view text)
    {
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<jsize>::max()))
        {
            JavaFailure::throwArgument(env, "A text longer than two gigabytes has no Java string.");
            return nullptr;
        }

        const auto size = static_cast<jsize>(text.size());
        jbyteArray bytes = env->NewByteArray(size);
        if (bytes == nullptr)
        {
            return nullptr;
        }

        env->SetByteArrayRegion(bytes, 0, size, reinterpret_cast<const jbyte*>(text.data()));

        const JavaSymbols& symbols = JavaSymbols::instance();
        auto created = static_cast<jstring>(env->NewObject(symbols.text, symbols.textFromBytes, bytes, symbols.utf8));
        env->DeleteLocalRef(bytes);
        return created;
    }
};

class JavaCalls
{
public:
    // A host function that throws answers null to Lua once its exception reached logcat.
    static const char* callHost(const char* argument, void* userdata)
    {
        const auto* function = static_cast<const JavaCallback*>(userdata);

        Attachment attachment;
        JNIEnv* env = attachment.env();
        if (env == nullptr)
        {
            return nullptr;
        }

        LocalFrame frame(env, 3);
        jstring jsonArgument = frame.isPushed() ? JavaText::toJava(env, argument) : nullptr;
        if (jsonArgument == nullptr)
        {
            JavaFailure::clear(env);
            return nullptr;
        }

        auto answer = static_cast<jstring>(env->CallObjectMethod(function->get(), JavaSymbols::instance().hostCall, jsonArgument));
        if (JavaFailure::clear(env) || answer == nullptr)
        {
            return nullptr;
        }

        std::optional<std::string> text = JavaText::toUtf8(env, answer);
        if (!text)
        {
            JavaFailure::clear(env);
            return nullptr;
        }

        answerBuffer() = std::move(*text);
        return answerBuffer().c_str();
    }

    static void deliverLine(const JavaCallback& sink, varn::log::Level level, std::string_view message)
    {
        Attachment attachment;
        JNIEnv* env = attachment.env();
        if (env == nullptr)
        {
            return;
        }

        LocalFrame frame(env, 2);
        jstring text = frame.isPushed() ? JavaText::toJava(env, message) : nullptr;
        if (text == nullptr)
        {
            JavaFailure::clear(env);
            return;
        }

        env->CallVoidMethod(sink.get(), JavaSymbols::instance().consoleLine, static_cast<jint>(level), text);
        JavaFailure::clear(env);
    }

    static void wake(void* userdata)
    {
        const auto* wake = static_cast<const JavaCallback*>(userdata);

        Attachment attachment;
        JNIEnv* env = attachment.env();
        if (env == nullptr)
        {
            return;
        }

        env->CallVoidMethod(wake->get(), JavaSymbols::instance().wakeRun);
        JavaFailure::clear(env);
    }

private:
    // The C API copies the answer as soon as the call returns, so a buffer of the calling thread owns it until then.
    static std::string& answerBuffer()
    {
        static thread_local std::string buffer;
        return buffer;
    }
};

class JavaConsole
{
public:
    // The sink owns the callback, so a line already on its way when another sink replaces it keeps the callback alive until it is delivered.
    static void install(JNIEnv* env, jobject sink)
    {
        if (sink == nullptr)
        {
            varn::log::Log::setSink({});
            return;
        }

        auto callback = std::make_shared<const JavaCallback>(env, sink);

        // clang-format off
        varn::log::Log::setSink([callback](varn::log::Level level, std::string_view message)
        {
            JavaCalls::deliverLine(*callback, level, message);
        });
        // clang-format on
    }
};

// The runtime that a `VarnRuntime` owns together with the callbacks of Java it reaches, which live until the runtime is freed since it can call them until then.
class JniRuntime
{
public:
    static constexpr jint kBadArgument = 2;

    explicit JniRuntime(varn_runtime* created)
        : runtime(created)
    {
    }

    // The runtime is freed before the members release their callbacks, so nothing calls one that is gone.
    ~JniRuntime()
    {
        varn_runtime_free(runtime);
    }

    JniRuntime(const JniRuntime&) = delete;
    JniRuntime& operator=(const JniRuntime&) = delete;

    // The handle that Kotlin keeps owns the runtime from `nativeNew` until `nativeFree`.
    static jlong toHandle(std::unique_ptr<JniRuntime> owner)
    {
        return reinterpret_cast<jlong>(owner.release());
    }

    static std::unique_ptr<JniRuntime> adopt(jlong handle)
    {
        return std::unique_ptr<JniRuntime>(from(handle));
    }

    // A closed `VarnRuntime` hands the handle zero, which reaches the C API as a null runtime.
    static JniRuntime* from(jlong handle)
    {
        return reinterpret_cast<JniRuntime*>(handle);
    }

    static varn_runtime* runtimeOf(jlong handle)
    {
        JniRuntime* owner = from(handle);
        return owner != nullptr ? owner->runtime : nullptr;
    }

    jint registerFunction(JNIEnv* env, const std::string& name, jobject function)
    {
        auto callback = std::make_unique<JavaCallback>(env, function);

        std::lock_guard<std::mutex> lock(functionsMutex);
        const jint code = varn_runtime_register(runtime, name.c_str(), &JavaCalls::callHost, callback.get());
        if (code == 0)
        {
            functions.push_back(std::move(callback));
        }

        return code;
    }

    void setWake(JNIEnv* env, jobject wake)
    {
        std::unique_ptr<JavaCallback> next = wake != nullptr ? std::make_unique<JavaCallback>(env, wake) : nullptr;

        // The runtime returns only once no call of the previous wake still runs, so that wake is released right after.
        std::lock_guard<std::mutex> lock(wakeMutex);
        varn_runtime_set_wake(runtime, next != nullptr ? &JavaCalls::wake : nullptr, next.get());
        wakeCallback = std::move(next);
    }

private:
    std::mutex functionsMutex;
    std::vector<std::unique_ptr<JavaCallback>> functions;
    std::mutex wakeMutex;
    std::unique_ptr<JavaCallback> wakeCallback;
    varn_runtime* runtime = nullptr;
};

class JniBindings
{
public:
    static bool resolve(JavaVM* vm, JNIEnv* env)
    {
        JavaSymbols& symbols = JavaSymbols::instance();
        symbols.vm = vm;

        JavaResolver resolver(env);
        symbols.text = resolver.pinClass("java/lang/String");
        symbols.invalidArgument = resolver.pinClass("java/lang/IllegalArgumentException");
        symbols.utf8 = resolver.pinStaticObject("java/nio/charset/StandardCharsets", "UTF_8", "Ljava/nio/charset/Charset;");
        symbols.textFromBytes = resolver.method("java/lang/String", "<init>", "([BLjava/nio/charset/Charset;)V");
        symbols.textBytes = resolver.method("java/lang/String", "getBytes", "(Ljava/nio/charset/Charset;)[B");
        symbols.hostCall = resolver.method("com/varn/VarnRuntime$HostFunction", "call", "(Ljava/lang/String;)Ljava/lang/String;");
        symbols.consoleLine = resolver.method("com/varn/VarnRuntime$ConsoleSink", "onLine", "(ILjava/lang/String;)V");
        symbols.wakeRun = resolver.method("java/lang/Runnable", "run", "()V");
        symbols.pollRan = resolver.field("com/varn/VarnRuntime$PollResult", "ran", "Z");
        symbols.pollPending = resolver.field("com/varn/VarnRuntime$PollResult", "pending", "Z");
        symbols.pollIdle = resolver.field("com/varn/VarnRuntime$PollResult", "idleMilliseconds", "J");

        if (!resolver.getMissing().empty())
        {
            varn::log::Log::error("The runtime could not reach " + resolver.getMissing() + ". Keep it in the rules of R8.");
            return false;
        }

        return true;
    }
};

} // namespace

extern "C"
{

    JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*)
    {
        JNIEnv* env = nullptr;
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || !JniBindings::resolve(vm, env))
        {
            return JNI_ERR;
        }

        // The HTTP transport and the data views resolve their Java classes here, while the app classloader is still the one in reach.
        varn::http::client::AndroidHttpBridge::publish(vm);
        if (!varn::dataview::DataViewJni::publish(env))
        {
            return JNI_ERR;
        }

        return JNI_VERSION_1_6;
    }

    JNIEXPORT jlong JNICALL Java_com_varn_VarnRuntime_nativeNew(JNIEnv*, jclass, jint taskThreads, jint ioThreads)
    {
        const varn_runtime_options options{static_cast<unsigned>(taskThreads), static_cast<unsigned>(ioThreads)};
        varn_runtime* runtime = varn_runtime_new_with_options(&options);
        if (runtime == nullptr)
        {
            return 0;
        }

        return JniRuntime::toHandle(std::make_unique<JniRuntime>(runtime));
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRegister(JNIEnv* env, jclass, jlong handle, jstring name, jobject function)
    {
        JniRuntime* owner = JniRuntime::from(handle);
        if (owner == nullptr)
        {
            return JniRuntime::kBadArgument;
        }

        const std::optional<std::string> nativeName = JavaText::toUtf8(env, name);
        if (!nativeName)
        {
            return JniRuntime::kBadArgument;
        }

        return owner->registerFunction(env, *nativeName, function);
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeEmit(JNIEnv* env, jclass, jlong handle, jstring name, jstring jsonArgument)
    {
        const std::optional<std::string> nativeName = JavaText::toUtf8(env, name);
        if (!nativeName)
        {
            return JniRuntime::kBadArgument;
        }

        const std::optional<std::string> nativeArgument = JavaText::toUtf8(env, jsonArgument);
        if (!nativeArgument)
        {
            return JniRuntime::kBadArgument;
        }

        return varn_runtime_emit(JniRuntime::runtimeOf(handle), nativeName->c_str(), nativeArgument->c_str());
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRetain(JNIEnv*, jclass, jlong handle)
    {
        return varn_runtime_retain(JniRuntime::runtimeOf(handle));
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRelease(JNIEnv*, jclass, jlong handle)
    {
        return varn_runtime_release(JniRuntime::runtimeOf(handle));
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRunFile(JNIEnv* env, jclass, jlong handle, jstring path)
    {
        const std::optional<std::string> nativePath = JavaText::toUtf8(env, path);
        if (!nativePath)
        {
            return JniRuntime::kBadArgument;
        }

        return varn_runtime_run_file(JniRuntime::runtimeOf(handle), nativePath->c_str());
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRunString(JNIEnv* env, jclass, jlong handle, jstring source, jstring chunkName)
    {
        const std::optional<std::string> nativeSource = JavaText::toUtf8(env, source);
        if (!nativeSource)
        {
            return JniRuntime::kBadArgument;
        }

        const std::optional<std::string> nativeChunk = JavaText::toUtf8(env, chunkName);
        if (!nativeChunk)
        {
            return JniRuntime::kBadArgument;
        }

        return varn_runtime_run_string(JniRuntime::runtimeOf(handle), nativeSource->c_str(), nativeChunk->c_str());
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeLoadFile(JNIEnv* env, jclass, jlong handle, jstring path)
    {
        const std::optional<std::string> nativePath = JavaText::toUtf8(env, path);
        if (!nativePath)
        {
            return JniRuntime::kBadArgument;
        }

        return varn_runtime_load_file(JniRuntime::runtimeOf(handle), nativePath->c_str());
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeLoadString(JNIEnv* env, jclass, jlong handle, jstring source, jstring chunkName)
    {
        const std::optional<std::string> nativeSource = JavaText::toUtf8(env, source);
        if (!nativeSource)
        {
            return JniRuntime::kBadArgument;
        }

        const std::optional<std::string> nativeChunk = JavaText::toUtf8(env, chunkName);
        if (!nativeChunk)
        {
            return JniRuntime::kBadArgument;
        }

        return varn_runtime_load_string(JniRuntime::runtimeOf(handle), nativeSource->c_str(), nativeChunk->c_str());
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativePoll(JNIEnv*, jclass, jlong handle)
    {
        return varn_runtime_poll(JniRuntime::runtimeOf(handle));
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativePollBudget(JNIEnv* env, jclass, jlong handle, jlong budgetNanoseconds, jobject result)
    {
        varn_poll_result polled{};
        const jint code = varn_runtime_poll_budget(JniRuntime::runtimeOf(handle), budgetNanoseconds, &polled);
        if (code == JniRuntime::kBadArgument || result == nullptr)
        {
            return code;
        }

        const JavaSymbols& symbols = JavaSymbols::instance();
        env->SetBooleanField(result, symbols.pollRan, polled.ran != 0 ? JNI_TRUE : JNI_FALSE);
        env->SetBooleanField(result, symbols.pollPending, polled.pending != 0 ? JNI_TRUE : JNI_FALSE);
        env->SetLongField(result, symbols.pollIdle, static_cast<jlong>(polled.idle_milliseconds));
        return code;
    }

    JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_nativeSetWake(JNIEnv* env, jclass, jlong handle, jobject wake)
    {
        JniRuntime* owner = JniRuntime::from(handle);
        if (owner != nullptr)
        {
            owner->setWake(env, wake);
        }
    }

    JNIEXPORT jlong JNICALL Java_com_varn_VarnRuntime_nativeIdle(JNIEnv*, jclass, jlong handle)
    {
        return static_cast<jlong>(varn_runtime_idle(JniRuntime::runtimeOf(handle)));
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeReleaseDataview(JNIEnv*, jclass, jlong handle, jlong view)
    {
        return varn_runtime_release_dataview(JniRuntime::runtimeOf(handle), reinterpret_cast<varn_dataview*>(view));
    }

    JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_nativeStop(JNIEnv*, jclass, jlong handle)
    {
        varn_runtime_stop(JniRuntime::runtimeOf(handle));
    }

    JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_nativeFree(JNIEnv*, jclass, jlong handle)
    {
        JniRuntime::adopt(handle).reset();
    }

    JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_nativeSetConsole(JNIEnv* env, jclass, jobject sink)
    {
        JavaConsole::install(env, sink);
    }

    JNIEXPORT jstring JNICALL Java_com_varn_VarnRuntime_nativeVersion(JNIEnv* env, jclass)
    {
        return JavaText::toJava(env, varn_version());
    }

} // extern "C"
