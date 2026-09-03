#include "varn/http/AndroidHttpBridge.h"
#include "varn/http/HttpClientPerform.h"

#include <stdexcept>
#include <string>
#include <vector>

// the platform http stack reached through jni, which is what makes an app's network_security_config.xml apply:
// trust anchors, certificate pinning and the cleartext policy are declared there and enforced on the way through,
// none of which a transport carrying its own tls stack can honour

namespace varn::http::client
{

namespace
{

// what the transport needs from java, resolved once while the app classloader is still the one in reach
class AndroidJniCache
{
public:
    JavaVM* vm = nullptr;
    jclass transport = nullptr;
    jclass response = nullptr;
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

    static AndroidJniCache& instance()
    {
        static AndroidJniCache cache;
        return cache;
    }
};

// the engine runs a request on a pool thread, which java does not know about until it is attached here
class Attachment
{
public:
    Attachment()
    {
        JavaVM* vm = AndroidJniCache::instance().vm;
        if (vm == nullptr)
        {
            throw std::runtime_error("[AndroidHttpClient] The java virtual machine was never published to the engine.");
        }

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

    static ResponseHeaders readHeaders(JNIEnv* env, jobjectArray names, jobjectArray values)
    {
        ResponseHeaders out;
        const jsize count = names != nullptr ? env->GetArrayLength(names) : 0;
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

// the arguments one call hands to java, kept together so both entry points marshal them the same way
class AndroidRequest
{
public:
    AndroidRequest(
        JNIEnv* env,
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body)
        : environment(env)
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

    ~AndroidRequest()
    {
        environment->DeleteLocalRef(jMethod);
        environment->DeleteLocalRef(jUrl);
        environment->DeleteLocalRef(jNames);
        environment->DeleteLocalRef(jValues);
        environment->DeleteLocalRef(jBody);
    }

    AndroidRequest(const AndroidRequest&) = delete;
    AndroidRequest& operator=(const AndroidRequest&) = delete;

    jstring jMethod = nullptr;
    jstring jUrl = nullptr;
    jobjectArray jNames = nullptr;
    jobjectArray jValues = nullptr;
    jbyteArray jBody = nullptr;

private:
    JNIEnv* environment = nullptr;
};

// what a streaming call hands back to the engine, reached from java through the handle the sink carries
struct StreamTarget
{
    const StreamResponseFn* onResponse = nullptr;
    const StreamChunkFn* onChunk = nullptr;
};

class AndroidHttpRunner
{
public:
    // a failure on the java side arrives through the response rather than as an exception crossing jni
    static void rethrowFailure(JNIEnv* env, jobject answer)
    {
        const AndroidJniCache& cache = AndroidJniCache::instance();
        auto message = static_cast<jstring>(env->GetObjectField(answer, cache.error));
        if (message == nullptr)
        {
            return;
        }

        const std::string described = AndroidHttpHelpers::toStdString(env, message);
        env->DeleteLocalRef(message);
        throw std::runtime_error(described);
    }

    // a java exception left pending would corrupt the next call, so it is cleared and reported as the failure it is
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

    static void readStatusAndHeaders(JNIEnv* env, jobject answer, ClientResponse& out)
    {
        const AndroidJniCache& cache = AndroidJniCache::instance();
        out.status = env->GetIntField(answer, cache.status);

        auto names = static_cast<jobjectArray>(env->GetObjectField(answer, cache.headerNames));
        auto values = static_cast<jobjectArray>(env->GetObjectField(answer, cache.headerValues));
        out.headers = AndroidHttpHelpers::readHeaders(env, names, values);

        env->DeleteLocalRef(names);
        env->DeleteLocalRef(values);
    }
};

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
        env->DeleteLocalRef(bytes);
    }

    env->DeleteLocalRef(answer);
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
    const AndroidJniCache& cache = AndroidJniCache::instance();

    const AndroidRequest request(env, method, url, headers, body);

    // the sink carries the address of this frame, which outlives the call it is passed to
    StreamTarget target{&onResponse, &onChunk};
    jobject sink = env->NewObject(cache.sink, cache.newSink, reinterpret_cast<jlong>(&target));

    // clang-format off
    jobject answer = env->CallStaticObjectMethod(
        cache.transport, cache.performStream,
        request.jMethod, request.jUrl, request.jNames, request.jValues, request.jBody,
        static_cast<jint>(options.timeoutSeconds),
        options.verifyTls ? JNI_TRUE : JNI_FALSE,
        static_cast<jlong>(options.maxResponseBytes),
        sink);
    // clang-format on

    env->DeleteLocalRef(sink);

    AndroidHttpRunner::rethrowPending(env);
    AndroidHttpRunner::rethrowFailure(env, answer);
    env->DeleteLocalRef(answer);
}

void AndroidHttpBridge::publish(JavaVM* vm)
{
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        return;
    }

    // a pool thread attached later sees only the bootstrap classloader, so every class is resolved and pinned here
    AndroidJniCache& cache = AndroidJniCache::instance();
    cache.vm = vm;
    cache.transport = static_cast<jclass>(env->NewGlobalRef(env->FindClass("com/varn/VarnHttp")));
    cache.response = static_cast<jclass>(env->NewGlobalRef(env->FindClass("com/varn/VarnHttp$Response")));
    cache.sink = static_cast<jclass>(env->NewGlobalRef(env->FindClass("com/varn/NativeChunkSink")));
    cache.text = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/lang/String")));
    cache.failure = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/lang/RuntimeException")));

    const char* performSignature =
        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[BIZJ)Lcom/varn/VarnHttp$Response;";
    const char* streamSignature =
        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[BIZJLcom/varn/VarnHttp$ChunkSink;)"
        "Lcom/varn/VarnHttp$Response;";

    cache.perform = env->GetStaticMethodID(cache.transport, "perform", performSignature);
    cache.performStream = env->GetStaticMethodID(cache.transport, "performStream", streamSignature);
    cache.newSink = env->GetMethodID(cache.sink, "<init>", "(J)V");
    cache.sinkHandle = env->GetFieldID(cache.sink, "handle", "J");

    cache.status = env->GetFieldID(cache.response, "status", "I");
    cache.headerNames = env->GetFieldID(cache.response, "headerNames", "[Ljava/lang/String;");
    cache.headerValues = env->GetFieldID(cache.response, "headerValues", "[Ljava/lang/String;");
    cache.body = env->GetFieldID(cache.response, "body", "[B");
    cache.error = env->GetFieldID(cache.response, "error", "Ljava/lang/String;");
}

} // namespace varn::http::client

namespace
{

// an escaping c++ exception would unwind through a java frame, so it is turned into the exception java expects
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
    }

    JNIEXPORT void JNICALL Java_com_varn_NativeChunkSink_onChunk(JNIEnv* env, jobject self, jbyteArray chunk, jint length)
    {
        varn::http::client::StreamTarget* target = NativeSinkTrampoline::target(env, self);
        if (target == nullptr || *target->onChunk == nullptr || length <= 0)
        {
            return;
        }

        // java reuses one buffer across chunks, so the valid prefix is copied out before the callback runs
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
    }

} // extern "C"
