#include "../../HttpClientPerform.h"

#include "../../HttpClientFailure.h"
#include "../../HttpClientTransfer.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

// This transport is the platform URL Loading System, so an app steers it through its own `Info.plist`.
// App Transport Security, exception domains, the minimum TLS version and certificate transparency all apply here.
// The trust store, system proxy and HTTP/2 come from the OS rather than from a bundle shipped with the engine.

namespace
{

class AppleHttpHelpers
{
public:
    AppleHttpHelpers() = delete;

    static NSString* toNsString(const std::string& text)
    {
        return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
    }

    static std::string toStdString(NSString* text)
    {
        if (text == nil)
        {
            return std::string();
        }

        const char* utf8 = [text UTF8String];
        return utf8 != nullptr ? std::string(utf8) : std::string();
    }

    static std::string lowered(const std::string& text)
    {
        std::string out = text;
        // clang-format off
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        // clang-format on
        return out;
    }

    // The URL Loading System decodes a compressed body before handing it over, so the headers describing the encoded form would contradict what the caller receives and are dropped.
    static bool describesEncodedBody(const std::string& name)
    {
        const std::string key = lowered(name);
        return key == "content-encoding" || key == "content-length";
    }

    static varn::http::client::ResponseHead headOf(NSHTTPURLResponse* response, const std::string& method)
    {
        varn::http::client::ResponseHead head;
        head.status = static_cast<int>(response.statusCode);
        bool encoded = false;

        for (NSString* name in response.allHeaderFields)
        {
            id value = response.allHeaderFields[name];
            const std::string key = toStdString(name);
            encoded = encoded || lowered(key) == "content-encoding";
            if ([value isKindOfClass:[NSString class]] && !describesEncodedBody(key))
            {
                head.headers.emplace_back(key, toStdString(value));
            }
        }

        // The length of a decoded body is known only when nothing was decoded.
        const bool bodiless = method == "HEAD" || head.status < 200 || head.status == 204 || head.status == 304;
        if (bodiless)
        {
            head.contentLength = 0;
        }
        else if (!encoded && response.expectedContentLength >= 0)
        {
            head.contentLength = static_cast<std::uint64_t>(response.expectedContentLength);
        }

        return head;
    }

    static varn::http::client::Error classify(NSError* failure, const varn::http::client::HttpClientTransfer& transfer)
    {
        using varn::http::client::Error;
        using varn::http::client::ErrorCode;

        const std::string message = "[AppleHttpClient] " + toStdString(failure.localizedDescription);
        if (![failure.domain isEqualToString:NSURLErrorDomain])
        {
            return Error{ErrorCode::Network, message};
        }

        switch (failure.code)
        {
        case NSURLErrorTimedOut:
            return transfer.timeoutError();
        case NSURLErrorCancelled:
            return Error{ErrorCode::Cancelled, message};
        case NSURLErrorBadURL:
        case NSURLErrorUnsupportedURL:
            return Error{ErrorCode::Invalid, message};
        case NSURLErrorSecureConnectionFailed:
        case NSURLErrorServerCertificateHasBadDate:
        case NSURLErrorServerCertificateUntrusted:
        case NSURLErrorServerCertificateHasUnknownRoot:
        case NSURLErrorServerCertificateNotYetValid:
        case NSURLErrorClientCertificateRejected:
        case NSURLErrorClientCertificateRequired:
        case NSURLErrorAppTransportSecurityRequiresSecureConnection:
            return Error{ErrorCode::Tls, message};
        default:
            return Error{ErrorCode::Network, message};
        }
    }
};

} // namespace

// What one task of a shared session reports back to the pool thread that waits for it.
@interface VarnHttpTask : NSObject
@property(nonatomic, assign) varn::http::client::HttpClientTransfer* transfer;
@property(nonatomic, assign) const varn::http::client::HeadFn* onHead;
@property(nonatomic, copy) NSString* method;
@property(nonatomic, assign) BOOL swallowed;
@property(nonatomic, strong) NSError* failure;
@property(nonatomic, copy) NSString* handlerFailure;
@property(atomic, assign) BOOL completed;
@property(nonatomic, strong) dispatch_semaphore_t done;
@end

@implementation VarnHttpTask
@end

// One delegate serves every task of a session and routes each callback to the task it belongs to.
@interface VarnHttpSessionDelegate : NSObject <NSURLSessionDataDelegate>
@property(nonatomic, assign) BOOL verifyTls;
- (void)track:(VarnHttpTask*)context forTask:(NSURLSessionTask*)task;
- (void)forgetTask:(NSURLSessionTask*)task;
@end

@implementation VarnHttpSessionDelegate
{
    NSMutableDictionary<NSNumber*, VarnHttpTask*>* tasks;
}

- (instancetype)init
{
    self = [super init];
    if (self != nil)
    {
        tasks = [NSMutableDictionary dictionary];
    }

    return self;
}

- (void)track:(VarnHttpTask*)context forTask:(NSURLSessionTask*)task
{
    @synchronized(self)
    {
        tasks[@(task.taskIdentifier)] = context;
    }
}

- (void)forgetTask:(NSURLSessionTask*)task
{
    @synchronized(self)
    {
        [tasks removeObjectForKey:@(task.taskIdentifier)];
    }
}

- (VarnHttpTask*)contextOf:(NSURLSessionTask*)task
{
    @synchronized(self)
    {
        return tasks[@(task.taskIdentifier)];
    }
}

// An untrusted certificate is accepted only by the session of the callers that opted out of verification for a development server.
- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completionHandler
{
    const BOOL serverTrust = [challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust];
    if (!serverTrust || self.verifyTls)
    {
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }

    NSURLCredential* credential = [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust];
    completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
}

// The URL Loading System follows a redirect on its own, while this client hands the 3xx to the caller like every other transport does.
- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler
{
    completionHandler(nil);
}

// A body that streams from the caller is read once, so the session is never handed a second stream to send it again.
- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task needNewBodyStream:(void (^)(NSInputStream*))completionHandler
{
    completionHandler(nil);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler
{
    VarnHttpTask* context = [self contextOf:dataTask];
    if (context == nil || ![response isKindOfClass:[NSHTTPURLResponse class]])
    {
        completionHandler(NSURLSessionResponseCancel);
        return;
    }

    // A handler is caller code, and letting it unwind through the queue of the URL Loading System would terminate the process.
    bool wanted = false;
    try
    {
        varn::http::client::ResponseHead head = AppleHttpHelpers::headOf(static_cast<NSHTTPURLResponse*>(response), AppleHttpHelpers::toStdString(context.method));
        wanted = (*context.onHead)(head);
    }
    catch (const std::exception& failure)
    {
        context.handlerFailure = AppleHttpHelpers::toNsString(failure.what());
    }
    catch (...)
    {
        context.handlerFailure = @"[AppleHttpClient] The response handler failed.";
    }

    context.swallowed = !wanted;
    completionHandler(wanted ? NSURLSessionResponseAllow : NSURLSessionResponseCancel);
}

// Each region of the data is handed over where it lies, so a body made of several buffers is never flattened into one.
// Handing it over waits while the loop holds a full window, which holds the queue of the session and is what makes the system stop reading the socket, since suspending a task that already receives does not.
- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask didReceiveData:(NSData*)data
{
    VarnHttpTask* context = [self contextOf:dataTask];
    if (context == nil)
    {
        return;
    }

    __block bool keep = true;
    [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange range, BOOL* stop) {
        keep = context.transfer->deliver(static_cast<const char*>(bytes), range.length);
        *stop = keep ? NO : YES;
    }];

    if (!keep)
    {
        [dataTask cancel];
    }
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error
{
    VarnHttpTask* context = [self contextOf:task];
    if (context == nil)
    {
        return;
    }

    context.failure = error;
    context.completed = YES;
    dispatch_semaphore_signal(context.done);
}

@end

namespace varn::http::client
{

// The two sessions of a client, one that verifies servers and one for the callers that opted out, so a connection trusted without verification never carries a request that asked for it.
// Each session pools its connections, which is what lets a request to a host it already reached skip the TCP and TLS handshakes.
class HttpClientConnections
{
public:
    HttpClientConnections() = default;

    HttpClientConnections(const HttpClientConnections&) = delete;
    HttpClientConnections& operator=(const HttpClientConnections&) = delete;

    // A session holds its delegate until it is invalidated, so it is invalidated here or both would leak.
    ~HttpClientConnections()
    {
        [verifying invalidateAndCancel];
        [insecure invalidateAndCancel];
    }

    NSURLSession* session(bool verifyTls)
    {
        std::lock_guard<std::mutex> lock(mutex);
        NSURLSession* __strong& slot = verifyTls ? verifying : insecure;
        if (slot == nil)
        {
            slot = open(verifyTls);
        }

        return slot;
    }

private:
    static NSURLSession* open(bool verifyTls)
    {
        NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.HTTPShouldSetCookies = NO;
        configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
        configuration.URLCache = nil;
        configuration.URLCredentialStorage = nil;
        configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;

        VarnHttpSessionDelegate* delegate = [[VarnHttpSessionDelegate alloc] init];
        delegate.verifyTls = verifyTls ? YES : NO;

        // A private serial queue keeps the callbacks off the main thread, which a waiting request may be blocking.
        NSOperationQueue* queue = [[NSOperationQueue alloc] init];
        queue.maxConcurrentOperationCount = 1;

        return [NSURLSession sessionWithConfiguration:configuration delegate:delegate delegateQueue:queue];
    }

    std::mutex mutex;
    NSURLSession* verifying = nil;
    NSURLSession* insecure = nil;
};

namespace
{

class AppleHttpRunner
{
public:
    AppleHttpRunner() = delete;

    // Runs one hop on the shared session and waits for it on the pool thread that called, answering the failure it ended with.
    static std::optional<Error> run(HttpClientConnections& connections, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead)
    {
        const Request& request = transfer.request();
        NSURL* parsed = [NSURL URLWithString:AppleHttpHelpers::toNsString(hop.url)];
        if (parsed == nil || parsed.scheme == nil)
        {
            return Error{ErrorCode::Invalid, "[AppleHttpClient] The URL \"" + hop.url + "\" could not be parsed."};
        }

        const std::string scheme = AppleHttpHelpers::lowered(AppleHttpHelpers::toStdString(parsed.scheme));
        if (scheme != "http" && scheme != "https")
        {
            return Error{ErrorCode::Invalid, "[AppleHttpClient] The URL \"" + hop.url + "\" must be an \"http\" or \"https\" address."};
        }

        NSMutableURLRequest* message = [NSMutableURLRequest requestWithURL:parsed];
        message.HTTPMethod = AppleHttpHelpers::toNsString(hop.method);
        message.HTTPShouldHandleCookies = NO;
        message.timeoutInterval = std::max(secondsLeft(transfer), 0.001);
        for (const auto& [name, value] : hop.headers)
        {
            [message addValue:AppleHttpHelpers::toNsString(value) forHTTPHeaderField:AppleHttpHelpers::toNsString(name)];
        }

        const bool streamed = hop.withBody && static_cast<bool>(request.bodySource);
        NSOutputStream* output = nil;
        if (streamed)
        {
            NSInputStream* input = nil;
            [NSStream getBoundStreamsWithBufferSize:kPieceBytes inputStream:&input outputStream:&output];
            message.HTTPBodyStream = input;
            if (request.bodyLength)
            {
                [message setValue:AppleHttpHelpers::toNsString(std::to_string(*request.bodyLength)) forHTTPHeaderField:@"Content-Length"];
            }
        }
        else if (hop.withBody && !request.body.empty())
        {
            message.HTTPBody = [NSData dataWithBytes:request.body.data() length:request.body.size()];
        }

        NSURLSession* session = connections.session(request.verifyTls);
        auto* delegate = static_cast<VarnHttpSessionDelegate*>(session.delegate);
        NSURLSessionDataTask* task = [session dataTaskWithRequest:message];

        VarnHttpTask* context = [[VarnHttpTask alloc] init];
        context.transfer = &transfer;
        context.onHead = &onHead;
        context.method = AppleHttpHelpers::toNsString(hop.method);
        context.done = dispatch_semaphore_create(0);
        [delegate track:context forTask:task];

        // clang-format off
        const bool interruptible = transfer.setInterrupt([task] { [task cancel]; });
        // clang-format on

        if (!interruptible)
        {
            [delegate forgetTask:task];
            return Error{ErrorCode::Cancelled, "[HttpClient] The request was cancelled."};
        }

        [task resume];

        std::optional<Error> sourceFailure;
        if (streamed)
        {
            sourceFailure = pump(output, context, transfer);
            if (sourceFailure)
            {
                [task cancel];
            }
        }

        // The loop ends the transfer at its deadline, and the wait ends there too for a host that stopped polling.
        bool timedOut = false;
        const auto left = std::chrono::duration_cast<std::chrono::nanoseconds>(transfer.deadline() - std::chrono::steady_clock::now()).count();
        if (dispatch_semaphore_wait(context.done, dispatch_time(DISPATCH_TIME_NOW, std::max<std::int64_t>(left, 0))) != 0)
        {
            timedOut = true;
            [task cancel];
            dispatch_semaphore_wait(context.done, DISPATCH_TIME_FOREVER);
        }

        transfer.clearInterrupt();
        [delegate forgetTask:task];

        if (sourceFailure)
        {
            return sourceFailure;
        }

        if (context.handlerFailure != nil)
        {
            return Error{ErrorCode::Callback, AppleHttpHelpers::toStdString(context.handlerFailure)};
        }

        if (transfer.halted())
        {
            return Error{ErrorCode::Cancelled, "[HttpClient] The request was cancelled."};
        }

        if (timedOut)
        {
            return transfer.timeoutError();
        }

        // A body nobody wanted was cancelled on purpose, which is no failure.
        if (context.swallowed || context.failure == nil)
        {
            return std::nullopt;
        }

        return AppleHttpHelpers::classify(context.failure, transfer);
    }

private:
    // The pieces a body that streams from the caller is written in.
    static constexpr NSUInteger kPieceBytes = 65536;

    static double secondsLeft(const HttpClientTransfer& transfer)
    {
        return std::chrono::duration<double>(transfer.deadline() - std::chrono::steady_clock::now()).count();
    }

    // Writes a body that streams from the caller into the stream the session reads, waiting for room rather than blocking on a stream the session may have closed.
    static std::optional<Error> pump(NSOutputStream* output, VarnHttpTask* context, HttpClientTransfer& transfer)
    {
        [output open];
        char buffer[kPieceBytes];
        std::optional<Error> failure;

        while (!failure)
        {
            std::size_t length = 0;
            try
            {
                length = transfer.readBody(buffer, sizeof(buffer));
            }
            catch (const HttpClientFailure& refused)
            {
                failure = refused.error();
                break;
            }

            if (length == 0)
            {
                break;
            }

            std::size_t written = 0;
            while (written < length)
            {
                if (context.completed || transfer.halted() || output.streamStatus == NSStreamStatusError || output.streamStatus == NSStreamStatusClosed)
                {
                    [output close];
                    return failure;
                }

                if (!output.hasSpaceAvailable)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                const NSInteger sent = [output write:reinterpret_cast<const uint8_t*>(buffer + written) maxLength:length - written];
                if (sent <= 0)
                {
                    [output close];
                    return failure;
                }

                written += static_cast<std::size_t>(sent);
            }
        }

        [output close];
        return failure;
    }
};

} // namespace

std::shared_ptr<HttpClientConnections> HttpClientPerform::connections()
{
    return std::make_shared<HttpClientConnections>();
}

void HttpClientPerform::perform(HttpClientConnections& connections, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead)
{
    std::optional<Error> failure;
    @autoreleasepool
    {
        failure = AppleHttpRunner::run(connections, hop, transfer, onHead);
    }

    if (failure)
    {
        throw HttpClientFailure(failure->code, failure->message);
    }
}

} // namespace varn::http::client
