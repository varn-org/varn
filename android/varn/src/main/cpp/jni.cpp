#include "varn/varn.h"

#include <jni.h>

namespace
{

class JniHelpers
{
public:
    static varn_runtime* asRuntime(jlong handle)
    {
        return reinterpret_cast<varn_runtime*>(handle);
    }
};

} // namespace

extern "C"
{

    JNIEXPORT jlong JNICALL Java_com_varn_VarnRuntime_nativeNew(JNIEnv*, jclass)
    {
        return reinterpret_cast<jlong>(varn_runtime_new());
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
    }

    JNIEXPORT jstring JNICALL Java_com_varn_VarnRuntime_nativeVersion(JNIEnv* env, jclass)
    {
        return env->NewStringUTF(varn_version());
    }

} // extern "C"
