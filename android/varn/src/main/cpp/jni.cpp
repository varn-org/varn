#include "varn/http/AndroidHttpBridge.h"
#include "varn/varn.h"

#include <jni.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

// one registered host function, holding the java callback alive for as long as its runtime is
struct HostBinding
{
    jobject callback = nullptr;
};

class JniHelpers
{
public:
    static varn_runtime* asRuntime(jlong handle)
    {
        return reinterpret_cast<varn_runtime*>(handle);
    }

    static void setVm(JavaVM* value)
    {
        vmStorage() = value;
    }

    static JavaVM*& vmStorage()
    {
        static JavaVM* stored = nullptr;
        return stored;
    }

    // lua only ever runs on the thread that entered the runtime, so this thread is normally attached already
    static JNIEnv* env(bool& attached)
    {
        attached = false;

        JavaVM* machine = vmStorage();
        if (machine == nullptr)
        {
            return nullptr;
        }

        JNIEnv* current = nullptr;
        if (machine->GetEnv(reinterpret_cast<void**>(&current), JNI_VERSION_1_6) == JNI_OK)
        {
            return current;
        }

        if (machine->AttachCurrentThreadAsDaemon(&current, nullptr) != JNI_OK)
        {
            return nullptr;
        }

        attached = true;
        return current;
    }

    static std::mutex& bindingsMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    static std::unordered_map<jlong, std::vector<HostBinding*>>& bindings()
    {
        static std::unordered_map<jlong, std::vector<HostBinding*>> map;
        return map;
    }

    // the c api copies the answer as soon as the call returns, so a per-thread buffer is enough to own it
    static std::string& resultBuffer()
    {
        static thread_local std::string buffer;
        return buffer;
    }

    static const char* invoke(const char* argument, void* userdata)
    {
        HostBinding* binding = static_cast<HostBinding*>(userdata);

        bool attached = false;
        JNIEnv* current = env(attached);
        if (current == nullptr)
        {
            return nullptr;
        }

        jclass type = current->GetObjectClass(binding->callback);
        jmethodID method = current->GetMethodID(type, "call", "(Ljava/lang/String;)Ljava/lang/String;");

        jstring jsonArgument = current->NewStringUTF(argument != nullptr ? argument : "null");
        jobject answer = current->CallObjectMethod(binding->callback, method, jsonArgument);
        current->DeleteLocalRef(jsonArgument);

        // an exception raised by the callback would corrupt the next jni call, so it is cleared and reported as no answer
        if (current->ExceptionCheck() == JNI_TRUE)
        {
            current->ExceptionDescribe();
            current->ExceptionClear();
            answer = nullptr;
        }

        const char* result = nullptr;
        if (answer != nullptr)
        {
            const char* text = current->GetStringUTFChars(static_cast<jstring>(answer), nullptr);
            resultBuffer() = text != nullptr ? text : "null";
            current->ReleaseStringUTFChars(static_cast<jstring>(answer), text);
            current->DeleteLocalRef(answer);
            result = resultBuffer().c_str();
        }

        if (attached)
        {
            vmStorage()->DetachCurrentThread();
        }

        return result;
    }

    static void releaseBindings(jlong handle)
    {
        std::vector<HostBinding*> owned;
        {
            std::lock_guard<std::mutex> lock(bindingsMutex());
            auto found = bindings().find(handle);
            if (found == bindings().end())
            {
                return;
            }

            owned = std::move(found->second);
            bindings().erase(found);
        }

        bool attached = false;
        JNIEnv* current = env(attached);
        for (HostBinding* binding : owned)
        {
            if (current != nullptr)
            {
                current->DeleteGlobalRef(binding->callback);
            }
            delete binding;
        }

        if (attached && current != nullptr)
        {
            vmStorage()->DetachCurrentThread();
        }
    }
};

} // namespace

extern "C"
{

    JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*)
    {
        JniHelpers::setVm(vm);

        // the http transport resolves its java classes here, while the app classloader is still the one in reach
        varn::http::client::AndroidHttpBridge::publish(vm);
        return JNI_VERSION_1_6;
    }

    JNIEXPORT jlong JNICALL Java_com_varn_VarnRuntime_nativeNew(JNIEnv*, jclass)
    {
        return reinterpret_cast<jlong>(varn_runtime_new());
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRegister(JNIEnv* env, jclass, jlong handle, jstring name, jobject callback)
    {
        if (name == nullptr || callback == nullptr)
        {
            return 2;
        }

        HostBinding* binding = new HostBinding();
        binding->callback = env->NewGlobalRef(callback);

        const char* nativeName = env->GetStringUTFChars(name, nullptr);
        const jint code = varn_runtime_register(JniHelpers::asRuntime(handle), nativeName, &JniHelpers::invoke, binding);
        env->ReleaseStringUTFChars(name, nativeName);

        if (code != 0)
        {
            env->DeleteGlobalRef(binding->callback);
            delete binding;
            return code;
        }

        std::lock_guard<std::mutex> lock(JniHelpers::bindingsMutex());
        JniHelpers::bindings()[handle].push_back(binding);
        return code;
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeEmit(JNIEnv* env, jclass, jlong handle, jstring name, jstring jsonArgument)
    {
        if (name == nullptr)
        {
            return 2;
        }

        const char* nativeName = env->GetStringUTFChars(name, nullptr);
        const char* nativeArgument = jsonArgument != nullptr ? env->GetStringUTFChars(jsonArgument, nullptr) : nullptr;

        const jint code = varn_runtime_emit(JniHelpers::asRuntime(handle), nativeName, nativeArgument);

        env->ReleaseStringUTFChars(name, nativeName);
        if (nativeArgument != nullptr)
        {
            env->ReleaseStringUTFChars(jsonArgument, nativeArgument);
        }

        return code;
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRetain(JNIEnv*, jclass, jlong handle)
    {
        return varn_runtime_retain(JniHelpers::asRuntime(handle));
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRelease(JNIEnv*, jclass, jlong handle)
    {
        return varn_runtime_release(JniHelpers::asRuntime(handle));
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRunFile(JNIEnv* env, jclass, jlong handle, jstring path)
    {
        // GetStringUTFChars requires a non-null jstring, so a null argument is forwarded as null for the C api to reject
        const char* nativePath = path != nullptr ? env->GetStringUTFChars(path, nullptr) : nullptr;
        const jint code = varn_runtime_run_file(JniHelpers::asRuntime(handle), nativePath);
        if (nativePath != nullptr)
        {
            env->ReleaseStringUTFChars(path, nativePath);
        }
        return code;
    }

    JNIEXPORT jint JNICALL Java_com_varn_VarnRuntime_nativeRunString(JNIEnv* env, jclass, jlong handle, jstring source, jstring chunkName)
    {
        const char* nativeSource = source != nullptr ? env->GetStringUTFChars(source, nullptr) : nullptr;
        const char* nativeChunk = chunkName != nullptr ? env->GetStringUTFChars(chunkName, nullptr) : nullptr;
        const jint code = varn_runtime_run_string(JniHelpers::asRuntime(handle), nativeSource, nativeChunk);
        if (nativeSource != nullptr)
        {
            env->ReleaseStringUTFChars(source, nativeSource);
        }
        if (nativeChunk != nullptr)
        {
            env->ReleaseStringUTFChars(chunkName, nativeChunk);
        }
        return code;
    }

    JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_nativeStop(JNIEnv*, jclass, jlong handle)
    {
        varn_runtime_stop(JniHelpers::asRuntime(handle));
    }

    JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_nativeFree(JNIEnv*, jclass, jlong handle)
    {
        varn_runtime_free(JniHelpers::asRuntime(handle));
        JniHelpers::releaseBindings(handle);
    }

    JNIEXPORT jstring JNICALL Java_com_varn_VarnRuntime_nativeVersion(JNIEnv* env, jclass)
    {
        return env->NewStringUTF(varn_version());
    }

} // extern "C"
