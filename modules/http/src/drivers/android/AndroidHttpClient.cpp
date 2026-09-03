#include "varn/http/AndroidHttpBridge.h"
#include "varn/http/HttpClientPerform.h"
#include "varn/log/Log.h"

#include <stdexcept>
#include <string>
#include <vector>

// The platform http stack reached through jni, which is what makes an app's network_security_config.xml apply.
// Trust anchors, certificate pinning and the cleartext policy are declared there and enforced by the platform.
// A transport carrying its own tls stack could honour none of them.

namespace varn::http::client
{

namespace
{

// What the transport needs from java, resolved once while the app classloader is still the one in reach.
class AndroidJniCache
{
public:
    JavaVM* vm = nullptr;
    jclass transport = nullptr;
    jclass sink = nullptr;
    jclass text = nullptr;
    jclass failure = nullptr;
    jmethodID perform = nullptr;
    jmethodID performStream = nullptr;
    jmethodID newSink = nullptr;
    jfieldID sinkHandle = nullptr;
    jfieldID status = nullptr;
    jfieldID headerNames = nullptr;
    jfieldID headerValues = nullptr;
    jfieldID body = nullptr;
    jfieldID error = nullptr;
    std::string unresolved;

    static AndroidJniCache& instance()
    {
        static AndroidJniCache cache;
        return cache;
    }

    // A stripped or renamed class leaves the transport unusable, and saying which one is missing is the whole diagnosis.
    void requireResolved() const
    {
        if (!unresolved.empty())
        {
            throw std::runtime_error("[AndroidHttpClient] The platform transport is unavailable because java " + unresolved + " could not be resolved.");
        }
    }
};

// The engine runs a request on a pool thread, which java does not know about until it is attached here.
class Attachment
{
public:
    Attachment()
    {
        AndroidJniCache::instance().requireResolved();

        JavaVM* vm = AndroidJniCache::instance().vm;
        if (vm->GetEnv(reinterpret_cast<void**>(&environment), JNI_VERSION_1_6) == JNI_OK)
        {
            return;
        }

        if (vm->AttachCurrentThreadAsDaemon(&environment, nullptr) != JNI_OK)
        {
            throw std::runtime_error("[AndroidHttpClient] The calling thread could not be attached to the java virtual machine.");
        }

        attached = true;
    }

    ~Attachment()
    {
        if (attached)
        {
            AndroidJniCache::instance().vm->DetachCurrentThread();
        }
    }

    Attachment(const Attachment&) = delete;
    Attachment& operator=(const Attachment&) = delete;

    JNIEnv* env() const { return environment; }

private:
    JNIEnv* environment = nullptr;
    bool attached = false;
};

// An engine thread never returns to java, so without a frame of its own every request would leak its local references.
class LocalFrame
{
public:
    LocalFrame(JNIEnv* env, jint capacity)
        : environment(env)
    {
        if (env->PushLocalFrame(capacity) != JNI_OK)
        {
            env->ExceptionClear();
            throw std::runtime_error("[AndroidHttpClient] The java local reference frame could not be pushed.");
        }
    }

    ~LocalFrame()
    {
        environment->PopLocalFrame(nullptr);
    }

    LocalFrame(const LocalFrame&) = delete;
    LocalFrame& operator=(const LocalFrame&) = delete;

private:
    JNIEnv* environment = nullptr;
};

class AndroidHttpHelpers
{
public:
    static std::string toStdString(JNIEnv* env, jstring value)
    {
        if (value == nullptr)
        {
            return std::string();
        }

        const char* utf8 = env->GetStringUTFChars(value, nullptr);
        std::string out = utf8 != nullptr ? std::string(utf8) : std::string();
        env->ReleaseStringUTFChars(value, utf8);
        return out;
    }

    static jobjectArray toStringArray(JNIEnv* env, const std::vector<std::string>& values)
    {
        const AndroidJniCache& cache = AndroidJniCache::instance();
        jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), cache.text, nullptr);

        for (jsize index = 0; index < static_cast<jsize>(values.size()); ++index)
        {
            jstring value = env->NewStringUTF(values[static_cast<std::size_t>(index)].c_str());
            env->SetObjectArrayElement(array, index, value);
            env->DeleteLocalRef(value);
        }

        return array;
    }

    // A header set has no bound, so each pair is released as it is read rather than left to the enclosing frame.
    static ResponseHeaders readHeaders(JNIEnv* env, jobjectArray names, jobjectArray values)
    {
        ResponseHeaders out;
        const jsize count = names != nullptr && values != nullptr ? env->GetArrayLength(names) : 0;
        out.reserve(static_cast<std::size_t>(count));

        for (jsize index = 0; index < count; ++index)
        {
            auto name = static_cast<jstring>(env->GetObjectArrayElement(names, index));
            auto value = static_cast<jstring>(env->GetObjectArrayElement(values, index));
            out.emplace_back(toStdString(env, name), toStdString(env, value));
            env->DeleteLocalRef(name);
            env->DeleteLocalRef(value);
        }

        return out;
    }

    static jbyteArray toByteArray(JNIEnv* env, const std::string& body)
    {
        jbyteArray array = env->NewByteArray(static_cast<jsize>(body.size()));
        if (!body.empty())
        {
            env->SetByteArrayRegion(array, 0, static_cast<jsize>(body.size()), reinterpret_cast<const jbyte*>(body.data()));
        }

        return array;
    }
};

// The arguments one call hands to java, marshalled the same way by both entry points and owned by the enclosing frame.
class AndroidRequest
{
public:
    AndroidRequest(
        JNIEnv* env,
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body)
    {
        std::vector<std::string> names;
        std::vector<std::string> values;
        names.reserve(headers.size());
        values.reserve(headers.size());

        for (const auto& [name, value] : headers)
        {
            names.push_back(name);
            values.push_back(value);
        }

        jMethod = env->NewStringUTF(method.c_str());
        jUrl = env->NewStringUTF(url.c_str());
        jNames = AndroidHttpHelpers::toStringArray(env, names);
        jValues = AndroidHttpHelpers::toStringArray(env, values);
        jBody = AndroidHttpHelpers::toByteArray(env, body);
    }

    jstring jMethod = nullptr;
    jstring jUrl = nullptr;
    jobjectArray jNames = nullptr;
    jobjectArray jValues = nullptr;
    jbyteArray jBody = nullptr;
};

// What a streaming call hands back to the engine, reached from java through the handle the sink carries.
struct StreamTarget
{
    const StreamResponseFn* onResponse = nullptr;
    const StreamChunkFn* onChunk = nullptr;
};

class AndroidHttpRunner
{
public:
    // A java exception left pending makes the next jni call undefined, so it is cleared and reported as the failure it is.
    static void rethrowPending(JNIEnv* env)
    {
        if (env->ExceptionCheck() != JNI_TRUE)
        {
            return;
        }

        env->ExceptionDescribe();
        env->ExceptionClear();
        throw std::runtime_error("[AndroidHttpClient] The platform http stack raised an exception.");
    }

    // A failure on the java side arrives through the response rather than as an exception crossing jni.
    static void rethrowFailure(JNIEnv* env, jobject answer)
    {
        if (answer == nullptr)
        {
            throw std::runtime_error("[AndroidHttpClient] The platform http stack returned no response.");
        }

        auto message = static_cast<jstring>(env->GetObjectField(answer, AndroidJniCache::instance().error));
        if (message == nullptr)
        {
            return;
        }

        throw std::runtime_error(AndroidHttpHelpers::toStdString(env, message));
    }

    static void readStatusAndHeaders(JNIEnv* env, jobject answer, ClientResponse& out)
    {
        const AndroidJniCache& cache = AndroidJniCache::instance();
        out.status = env->GetIntField(answer, cache.status);

        auto names = static_cast<jobjectArray>(env->GetObjectField(answer, cache.headerNames));
        auto values = static_cast<jobjectArray>(env->GetObjectField(answer, cache.headerValues));
        out.headers = AndroidHttpHelpers::readHeaders(env, names, values);
    }
};

// One frame holds the request strings, both header arrays, the body, the answer and a little room to read it back.
constexpr jint kLocalFrameCapacity = 16;

} // namespace

ClientResponse HttpClientPerform::perform(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const ClientRequestOptions& options)
{
    Attachment attachment;
    JNIEnv* env = attachment.env();
    const LocalFrame frame(env, kLocalFrameCapacity);
    const AndroidJniCache& cache = AndroidJniCache::instance();

    const AndroidRequest request(env, method, url, headers, body);

    // clang-format off
    jobject answer = env->CallStaticObjectMethod(
        cache.transport, cache.perform,
        request.jMethod, request.jUrl, request.jNames, request.jValues, request.jBody,
        static_cast<jint>(options.timeoutSeconds),
        options.verifyTls ? JNI_TRUE : JNI_FALSE,
        static_cast<jlong>(options.maxResponseBytes));
    // clang-format on

    AndroidHttpRunner::rethrowPending(env);
    AndroidHttpRunner::rethrowFailure(env, answer);

    ClientResponse out;
    AndroidHttpRunner::readStatusAndHeaders(env, answer, out);

    auto bytes = static_cast<jbyteArray>(env->GetObjectField(answer, cache.body));
    if (bytes != nullptr)
    {
        const jsize length = env->GetArrayLength(bytes);
        out.body.resize(static_cast<std::size_t>(length));
        env->GetByteArrayRegion(bytes, 0, length, reinterpret_cast<jbyte*>(out.body.data()));
    }

    return out;
}

void HttpClientPerform::performStream(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const ClientRequestOptions& options,
    const StreamResponseFn& onResponse,
    const StreamChunkFn& onChunk)
{
    Attachment attachment;
    JNIEnv* env = attachment.env();
    const LocalFrame frame(env, kLocalFrameCapacity);
    const AndroidJniCache& cache = AndroidJniCache::instance();

    const AndroidRequest request(env, method, url, headers, body);

    // Java holds the sink only for the length of the call, so the target may live on this frame.
    StreamTarget target{&onResponse, &onChunk};
    jobject sink = env->NewObject(cache.sink, cache.newSink, reinterpret_cast<jlong>(&target));
    AndroidHttpRunner::rethrowPending(env);

    // clang-format off
    jobject answer = env->CallStaticObjectMethod(
        cache.transport, cache.performStream,
        request.jMethod, request.jUrl, request.jNames, request.jValues, request.jBody,
        static_cast<jint>(options.timeoutSeconds),
        options.verifyTls ? JNI_TRUE : JNI_FALSE,
        static_cast<jlong>(options.maxResponseBytes),
        sink);
    // clang-format on

    AndroidHttpRunner::rethrowPending(env);
    AndroidHttpRunner::rethrowFailure(env, answer);
}

void AndroidHttpBridge::publish(JavaVM* vm)
{
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        varn::log::Log::error("http", "the java virtual machine handed no environment to the http transport");
        return;
    }

    AndroidJniCache& cache = AndroidJniCache::instance();
    cache.vm = vm;

    // clang-format off
    const auto pinClass = [env, &cache](const char* name) -> jclass
    {
        jclass found = env->FindClass(name);
        if (found == nullptr)
        {
            env->ExceptionClear();
            cache.unresolved = std::string("class ") + name;
            return nullptr;
        }

        jclass pinned = static_cast<jclass>(env->NewGlobalRef(found));
        env->DeleteLocalRef(found);
        return pinned;
    };
    // clang-format on

    // A pool thread attached later sees only the bootstrap classloader, so every class is resolved and pinned here.
    cache.transport = pinClass("com/varn/VarnHttp");
    cache.sink = pinClass("com/varn/NativeChunkSink");
    cache.text = pinClass("java/lang/String");
    cache.failure = pinClass("java/lang/RuntimeException");

    jclass response = env->FindClass("com/varn/VarnHttp$Response");
    if (response == nullptr)
    {
        env->ExceptionClear();
        cache.unresolved = "class com/varn/VarnHttp$Response";
    }

    if (!cache.unresolved.empty())
    {
        varn::log::Log::error("http", "the platform http transport could not resolve java " + cache.unresolved);
        return;
    }

    const char* performSignature =
        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[BIZJ)Lcom/varn/VarnHttp$Response;";
    const char* streamSignature =
        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[BIZJLcom/varn/VarnHttp$ChunkSink;)"
        "Lcom/varn/VarnHttp$Response;";

    cache.perform = env->GetStaticMethodID(cache.transport, "perform", performSignature);
    cache.performStream = env->GetStaticMethodID(cache.transport, "performStream", streamSignature);
    cache.newSink = env->GetMethodID(cache.sink, "<init>", "(J)V");
    cache.sinkHandle = env->GetFieldID(cache.sink, "handle", "J");

    cache.status = env->GetFieldID(response, "status", "I");
    cache.headerNames = env->GetFieldID(response, "headerNames", "[Ljava/lang/String;");
    cache.headerValues = env->GetFieldID(response, "headerValues", "[Ljava/lang/String;");
    cache.body = env->GetFieldID(response, "body", "[B");
    cache.error = env->GetFieldID(response, "error", "Ljava/lang/String;");
    env->DeleteLocalRef(response);

    // A renamed member is as fatal as a missing class, and the two are reported the same way.
    const bool resolved = cache.perform != nullptr && cache.performStream != nullptr && cache.newSink != nullptr &&
                          cache.sinkHandle != nullptr && cache.status != nullptr && cache.headerNames != nullptr &&
                          cache.headerValues != nullptr && cache.body != nullptr && cache.error != nullptr;

    if (!resolved)
    {
        env->ExceptionClear();
        cache.unresolved = "members of com/varn/VarnHttp";
        varn::log::Log::error("http", "the platform http transport could not resolve java " + cache.unresolved);
    }
}

} // namespace varn::http::client

namespace
{

// An escaping c++ exception would unwind through a java frame, so it is turned into the exception java expects.
class NativeSinkTrampoline
{
public:
    static varn::http::client::StreamTarget* target(JNIEnv* env, jobject self)
    {
        const jlong handle = env->GetLongField(self, varn::http::client::AndroidJniCache::instance().sinkHandle);
        return reinterpret_cast<varn::http::client::StreamTarget*>(handle);
    }

    static void reportAsJavaFailure(JNIEnv* env, const char* described)
    {
        env->ThrowNew(varn::http::client::AndroidJniCache::instance().failure, described);
    }
};

} // namespace

extern "C"
{

    JNIEXPORT void JNICALL Java_com_varn_NativeChunkSink_onResponse(
        JNIEnv* env, jobject self, jint status, jobjectArray headerNames, jobjectArray headerValues)
    {
        varn::http::client::StreamTarget* target = NativeSinkTrampoline::target(env, self);
        if (target == nullptr || *target->onResponse == nullptr)
        {
            return;
        }

        try
        {
            const varn::http::client::ResponseHeaders headers =
                varn::http::client::AndroidHttpHelpers::readHeaders(env, headerNames, headerValues);
            (*target->onResponse)(static_cast<int>(status), headers);
        }
        catch (const std::exception& failure)
        {
            NativeSinkTrampoline::reportAsJavaFailure(env, failure.what());
        }
        catch (...)
        {
            NativeSinkTrampoline::reportAsJavaFailure(env, "[AndroidHttpClient] The response handler failed.");
        }
    }

    JNIEXPORT void JNICALL Java_com_varn_NativeChunkSink_onChunk(JNIEnv* env, jobject self, jbyteArray chunk, jint length)
    {
        varn::http::client::StreamTarget* target = NativeSinkTrampoline::target(env, self);
        if (target == nullptr || *target->onChunk == nullptr || length <= 0)
        {
            return;
        }

        // Java reuses one buffer across chunks, so the valid prefix is copied out before the callback runs.
        std::string bytes(static_cast<std::size_t>(length), '\0');
        env->GetByteArrayRegion(chunk, 0, length, reinterpret_cast<jbyte*>(bytes.data()));

        try
        {
            (*target->onChunk)(bytes.data(), bytes.size());
        }
        catch (const std::exception& failure)
        {
            NativeSinkTrampoline::reportAsJavaFailure(env, failure.what());
        }
        catch (...)
        {
            NativeSinkTrampoline::reportAsJavaFailure(env, "[AndroidHttpClient] The chunk handler failed.");
        }
    }

} // extern "C"
