#include "../../HttpClientPerform.h"

#include "../../HttpClientFailure.h"
#include "../../HttpClientTransfer.h"
#include "varn/http/AndroidHttpBridge.h"
#include "varn/log/Log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// This transport is the platform HTTP stack reached through JNI, which is what makes an app's `network_security_config.xml` apply.
// Trust anchors, certificate pinning and the cleartext policy are declared there and enforced by the platform.
// A transport carrying its own TLS stack could honour none of them.

namespace varn::http::client
{

// The platform keeps the pool of connections itself, so a client holds nothing of its own here.
class HttpClientConnections
{
};

namespace
{

// The kinds of failure the Java side reports, numbered as `VarnHttp` numbers them.
constexpr jint kKindTls = 2;
constexpr jint kKindTimeout = 3;
constexpr jint kKindInvalid = 4;

// What the transport needs from Java, resolved once while the app classloader is still the one in reach.
class AndroidJniCache
{
public:
    JavaVM* vm = nullptr;
    jclass transport = nullptr;
    jclass exchange = nullptr;
    jclass text = nullptr;
    jclass failure = nullptr;
    jmethodID perform = nullptr;
    jmethodID newExchange = nullptr;
    jmethodID cancel = nullptr;
    jfieldID exchangeHandle = nullptr;
    jfieldID error = nullptr;
    jfieldID kind = nullptr;
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
            throw HttpClientFailure(ErrorCode::Unavailable, "[AndroidHttpClient] The platform transport is unavailable because the Java " + unresolved + " could not be resolved.");
        }
    }
};

// The engine runs a request on a pool thread, which Java does not know about until it is attached here.
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
            throw HttpClientFailure(ErrorCode::Unavailable, "[AndroidHttpClient] The calling thread could not be attached to the Java virtual machine.");
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

// An engine thread never returns to Java, so without a frame of its own every request would leak its local references.
class LocalFrame
{
public:
    LocalFrame(JNIEnv* env, jint capacity)
        : environment(env)
    {
        if (env->PushLocalFrame(capacity) != JNI_OK)
        {
            env->ExceptionClear();
            throw HttpClientFailure(ErrorCode::Unavailable, "[AndroidHttpClient] The Java local reference frame could not be pushed.");
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

// A global reference to the exchange, which the thread that cancels reaches while the request runs and which is released once nothing can.
class GlobalExchange
{
public:
    GlobalExchange(JNIEnv* env, jobject exchange)
        : environment(env)
        , reference(env->NewGlobalRef(exchange))
    {
    }

    ~GlobalExchange()
    {
        environment->DeleteGlobalRef(reference);
    }

    GlobalExchange(const GlobalExchange&) = delete;
    GlobalExchange& operator=(const GlobalExchange&) = delete;

    jobject get() const { return reference; }

private:
    JNIEnv* environment = nullptr;
    jobject reference = nullptr;
};

// Clears the interrupt of the transfer before the exchange it reaches is released.
class InterruptScope
{
public:
    explicit InterruptScope(HttpClientTransfer& transfer)
        : transfer(transfer)
    {
    }

    ~InterruptScope()
    {
        transfer.clearInterrupt();
    }

    InterruptScope(const InterruptScope&) = delete;
    InterruptScope& operator=(const InterruptScope&) = delete;

private:
    HttpClientTransfer& transfer;
};

// What the Java side of one request reaches through the handle of its exchange.
struct ExchangeTarget
{
    HttpClientTransfer* transfer = nullptr;
    const HeadFn* onHead = nullptr;
    std::optional<Error> failure;
};

class AndroidHttpHelpers
{
public:
    AndroidHttpHelpers() = delete;

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
    static Headers readHeaders(JNIEnv* env, jobjectArray names, jobjectArray values)
    {
        Headers out;
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
        if (body.empty())
        {
            return nullptr;
        }

        jbyteArray array = env->NewByteArray(static_cast<jsize>(body.size()));
        env->SetByteArrayRegion(array, 0, static_cast<jsize>(body.size()), reinterpret_cast<const jbyte*>(body.data()));
        return array;
    }

    static ExchangeTarget* target(JNIEnv* env, jobject self)
    {
        const jlong handle = env->GetLongField(self, AndroidJniCache::instance().exchangeHandle);
        return reinterpret_cast<ExchangeTarget*>(handle);
    }
};

// The arguments one hop hands to Java, owned by the enclosing frame.
class AndroidRequest
{
public:
    AndroidRequest(JNIEnv* env, const Hop& hop, const Request& request)
    {
        std::vector<std::string> names;
        std::vector<std::string> values;
        names.reserve(hop.headers.size());
        values.reserve(hop.headers.size());

        for (const auto& [name, value] : hop.headers)
        {
            names.push_back(name);
            values.push_back(value);
        }

        jMethod = env->NewStringUTF(hop.method.c_str());
        jUrl = env->NewStringUTF(hop.url.c_str());
        jNames = AndroidHttpHelpers::toStringArray(env, names);
        jValues = AndroidHttpHelpers::toStringArray(env, values);
        jBody = hop.withBody ? AndroidHttpHelpers::toByteArray(env, request.body) : nullptr;
    }

    jstring jMethod = nullptr;
    jstring jUrl = nullptr;
    jobjectArray jNames = nullptr;
    jobjectArray jValues = nullptr;
    jbyteArray jBody = nullptr;
};

class AndroidHttpRunner
{
public:
    AndroidHttpRunner() = delete;

    // A Java exception left pending makes the next JNI call undefined, so it is cleared and reported as the failure it is.
    static void rethrowPending(JNIEnv* env)
    {
        if (env->ExceptionCheck() != JNI_TRUE)
        {
            return;
        }

        env->ExceptionDescribe();
        env->ExceptionClear();
        throw HttpClientFailure(ErrorCode::Network, "[AndroidHttpClient] The platform HTTP stack raised an exception.");
    }

    // A failure on the Java side arrives through the outcome rather than as an exception crossing JNI.
    static void rethrowFailure(JNIEnv* env, jobject outcome, const HttpClientTransfer& transfer)
    {
        if (outcome == nullptr)
        {
            throw HttpClientFailure(ErrorCode::Network, "[AndroidHttpClient] The platform HTTP stack returned no outcome.");
        }

        const AndroidJniCache& cache = AndroidJniCache::instance();
        auto message = static_cast<jstring>(env->GetObjectField(outcome, cache.error));
        if (message == nullptr)
        {
            return;
        }

        const jint kind = env->GetIntField(outcome, cache.kind);
        if (kind == kKindTimeout)
        {
            throw HttpClientFailure(ErrorCode::Timeout, transfer.timeoutError().message);
        }

        const ErrorCode code = kind == kKindTls ? ErrorCode::Tls : kind == kKindInvalid ? ErrorCode::Invalid
                                                                                        : ErrorCode::Network;
        throw HttpClientFailure(code, AndroidHttpHelpers::toStdString(env, message));
    }

    // The platform stack counts its timeouts in whole milliseconds, so what is left of the deadline is rounded up to one and kept within what an integer of Java holds.
    static jint millisecondsLeft(const HttpClientTransfer& transfer)
    {
        const double left = std::chrono::duration<double, std::milli>(transfer.deadline() - std::chrono::steady_clock::now()).count();
        if (left <= 0)
        {
            throw HttpClientFailure(ErrorCode::Timeout, transfer.timeoutError().message);
        }

        return static_cast<jint>(std::clamp(std::ceil(left), 1.0, static_cast<double>(std::numeric_limits<jint>::max())));
    }

    // Cancels the exchange from whatever thread ends the transfer, which may be one Java has never seen.
    static void cancel(jobject exchange)
    {
        try
        {
            Attachment attachment;
            JNIEnv* env = attachment.env();
            env->CallVoidMethod(exchange, AndroidJniCache::instance().cancel);
            env->ExceptionClear();
        }
        catch (const std::exception& failure)
        {
            varn::log::Log::error("http", std::string("The request could not be cancelled: ") + failure.what());
        }
    }
};

// One frame holds the request strings, both header arrays, the body, the exchange, the outcome and a little room to read it back.
constexpr jint kLocalFrameCapacity = 16;

} // namespace

std::shared_ptr<HttpClientConnections> HttpClientPerform::connections()
{
    return std::make_shared<HttpClientConnections>();
}

void HttpClientPerform::perform(HttpClientConnections& /*connections*/, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead)
{
    Attachment attachment;
    JNIEnv* env = attachment.env();
    const LocalFrame frame(env, kLocalFrameCapacity);
    const AndroidJniCache& cache = AndroidJniCache::instance();
    const Request& request = transfer.request();
    const AndroidRequest arguments(env, hop, request);

    // Java holds the exchange only for the length of the call, so the target may live on this frame.
    ExchangeTarget target{&transfer, &onHead, std::nullopt};
    jobject exchange = env->NewObject(cache.exchange, cache.newExchange, reinterpret_cast<jlong>(&target));
    AndroidHttpRunner::rethrowPending(env);

    const GlobalExchange global(env, exchange);
    const InterruptScope scope(transfer);
    jobject reachable = global.get();

    // clang-format off
    if (!transfer.setInterrupt([reachable] { AndroidHttpRunner::cancel(reachable); }))
    {
        throw HttpClientFailure(ErrorCode::Cancelled, "[HttpClient] The request was cancelled.");
    }

    const bool streamed = hop.withBody && static_cast<bool>(request.bodySource);
    jobject outcome = env->CallStaticObjectMethod(
        cache.transport, cache.perform,
        arguments.jMethod, arguments.jUrl, arguments.jNames, arguments.jValues, arguments.jBody,
        streamed ? JNI_TRUE : JNI_FALSE,
        static_cast<jlong>(streamed && request.bodyLength ? static_cast<jlong>(*request.bodyLength) : -1),
        AndroidHttpRunner::millisecondsLeft(transfer),
        request.verifyTls ? JNI_TRUE : JNI_FALSE,
        exchange);
    // clang-format on

    AndroidHttpRunner::rethrowPending(env);

    // A failure of the engine's own callbacks is the real cause of whatever Java reported after it.
    if (target.failure)
    {
        throw HttpClientFailure(target.failure->code, target.failure->message);
    }

    if (transfer.halted())
    {
        throw HttpClientFailure(ErrorCode::Cancelled, "[HttpClient] The request was cancelled.");
    }

    AndroidHttpRunner::rethrowFailure(env, outcome, transfer);
}

void AndroidHttpBridge::publish(JavaVM* vm)
{
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        varn::log::Log::error("http", "The Java virtual machine handed no environment to the HTTP transport.");
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
            cache.unresolved = std::string("class \"") + name + "\"";
            return nullptr;
        }

        jclass pinned = static_cast<jclass>(env->NewGlobalRef(found));
        env->DeleteLocalRef(found);
        return pinned;
    };
    // clang-format on

    // A pool thread attached later sees only the bootstrap classloader, so every class is resolved and pinned here.
    cache.transport = pinClass("com/varn/VarnHttp");
    cache.exchange = pinClass("com/varn/NativeExchange");
    cache.text = pinClass("java/lang/String");
    cache.failure = pinClass("java/lang/IllegalStateException");

    jclass outcome = env->FindClass("com/varn/VarnHttp$Outcome");
    if (outcome == nullptr)
    {
        env->ExceptionClear();
        cache.unresolved = "class \"com/varn/VarnHttp$Outcome\"";
    }

    if (!cache.unresolved.empty())
    {
        varn::log::Log::error("http", "The platform HTTP transport could not resolve the Java " + cache.unresolved + ".");
        return;
    }

    const char* performSignature =
        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[BZJIZLcom/varn/NativeExchange;)"
        "Lcom/varn/VarnHttp$Outcome;";

    cache.perform = env->GetStaticMethodID(cache.transport, "perform", performSignature);
    cache.newExchange = env->GetMethodID(cache.exchange, "<init>", "(J)V");
    cache.cancel = env->GetMethodID(cache.exchange, "cancel", "()V");
    cache.exchangeHandle = env->GetFieldID(cache.exchange, "handle", "J");
    cache.error = env->GetFieldID(outcome, "error", "Ljava/lang/String;");
    cache.kind = env->GetFieldID(outcome, "kind", "I");
    env->DeleteLocalRef(outcome);

    // A renamed member is as fatal as a missing class, and the two are reported the same way.
    const bool resolved = cache.perform != nullptr && cache.newExchange != nullptr && cache.cancel != nullptr &&
                          cache.exchangeHandle != nullptr && cache.error != nullptr && cache.kind != nullptr;

    if (!resolved)
    {
        env->ExceptionClear();
        cache.unresolved = "members of \"com/varn/VarnHttp\"";
        varn::log::Log::error("http", "The platform HTTP transport could not resolve the Java " + cache.unresolved + ".");
    }
}

namespace
{

// A C++ exception must never unwind through a Java frame, so each entry point records the failure for the engine and tells Java to stop.
class NativeExchangeTrampoline
{
public:
    NativeExchangeTrampoline() = delete;

    static jboolean onHead(JNIEnv* env, jobject self, jint status, jobjectArray headerNames, jobjectArray headerValues, jlong contentLength)
    {
        ExchangeTarget* target = AndroidHttpHelpers::target(env, self);
        if (target == nullptr)
        {
            return JNI_FALSE;
        }

        try
        {
            ResponseHead head;
            head.status = static_cast<int>(status);
            head.headers = AndroidHttpHelpers::readHeaders(env, headerNames, headerValues);
            if (contentLength >= 0)
            {
                head.contentLength = static_cast<std::uint64_t>(contentLength);
            }

            return (*target->onHead)(head) ? JNI_TRUE : JNI_FALSE;
        }
        catch (const std::exception& failure)
        {
            target->failure = Error{ErrorCode::Callback, failure.what()};
        }
        catch (...)
        {
            target->failure = Error{ErrorCode::Callback, "[AndroidHttpClient] The response handler failed."};
        }

        return JNI_FALSE;
    }

    // The elements of the array are read in place where the platform allows it, so a chunk is copied once, into the engine.
    static jboolean onChunk(JNIEnv* env, jobject self, jbyteArray chunk, jint length)
    {
        ExchangeTarget* target = AndroidHttpHelpers::target(env, self);
        if (target == nullptr)
        {
            return JNI_FALSE;
        }

        if (length <= 0)
        {
            return JNI_TRUE;
        }

        jbyte* bytes = env->GetByteArrayElements(chunk, nullptr);
        if (bytes == nullptr)
        {
            env->ExceptionClear();
            target->failure = Error{ErrorCode::Network, "[AndroidHttpClient] The chunk could not be read from Java."};
            return JNI_FALSE;
        }

        bool keep = false;
        try
        {
            keep = target->transfer->deliver(reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(length));
        }
        catch (const std::exception& failure)
        {
            target->failure = Error{ErrorCode::Network, failure.what()};
        }

        env->ReleaseByteArrayElements(chunk, bytes, JNI_ABORT);
        return keep ? JNI_TRUE : JNI_FALSE;
    }

    // Fills the buffer Java writes from with the next piece of a body that streams from the caller, raising in Java to stop a body that failed.
    static jint readBody(JNIEnv* env, jobject self, jbyteArray buffer)
    {
        ExchangeTarget* target = AndroidHttpHelpers::target(env, self);
        if (target == nullptr)
        {
            return -1;
        }

        const jsize capacity = env->GetArrayLength(buffer);
        jbyte* bytes = env->GetByteArrayElements(buffer, nullptr);
        if (bytes == nullptr)
        {
            env->ExceptionClear();
            target->failure = Error{ErrorCode::Network, "[AndroidHttpClient] The body buffer could not be reached from Java."};
            return -1;
        }

        std::size_t written = 0;
        try
        {
            written = target->transfer->readBody(reinterpret_cast<char*>(bytes), static_cast<std::size_t>(capacity));
        }
        catch (const HttpClientFailure& failure)
        {
            target->failure = failure.error();
        }

        env->ReleaseByteArrayElements(buffer, bytes, target->failure ? JNI_ABORT : 0);
        if (target->failure)
        {
            env->ThrowNew(AndroidJniCache::instance().failure, target->failure->message.c_str());
            return -1;
        }

        return static_cast<jint>(written);
    }
};

} // namespace

} // namespace varn::http::client

extern "C"
{

    JNIEXPORT jboolean JNICALL Java_com_varn_NativeExchange_onHead(JNIEnv* env, jobject self, jint status, jobjectArray headerNames, jobjectArray headerValues, jlong contentLength)
    {
        return varn::http::client::NativeExchangeTrampoline::onHead(env, self, status, headerNames, headerValues, contentLength);
    }

    JNIEXPORT jboolean JNICALL Java_com_varn_NativeExchange_onChunk(JNIEnv* env, jobject self, jbyteArray chunk, jint length)
    {
        return varn::http::client::NativeExchangeTrampoline::onChunk(env, self, chunk, length);
    }

    JNIEXPORT jint JNICALL Java_com_varn_NativeExchange_readBody(JNIEnv* env, jobject self, jbyteArray buffer)
    {
        return varn::http::client::NativeExchangeTrampoline::readBody(env, self, buffer);
    }

} // extern "C"
