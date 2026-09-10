#include "varn/http/HttpClientPerform.h"

#import <Foundation/Foundation.h>

#include <cctype>
#include <stdexcept>
#include <string>

// The platform url loading system, so an app steers this transport through its own Info.plist.
// App transport security, exception domains, the minimum tls version and certificate transparency all apply here.
// The trust store, system proxy and http/2 come from the os rather than from a bundle shipped with the engine.

namespace
{

class AppleHttpHelpers
{
public:
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

    static NSMutableURLRequest* buildRequest(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const varn::http::client::ClientRequestOptions& options)
    {
        NSURL* parsed = [NSURL URLWithString:toNsString(url)];
        if (parsed == nil || parsed.scheme == nil)
        {
            throw std::runtime_error("[AppleHttpClient] The URL could not be parsed.");
        }

        const std::string scheme = toStdString([parsed.scheme lowercaseString]);
        if (scheme != "http" && scheme != "https")
        {
            throw std::runtime_error("[AppleHttpClient] The URL scheme must be http or https.");
        }

        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:parsed];
        request.HTTPMethod = toNsString(method);
        request.timeoutInterval = static_cast<NSTimeInterval>(options.timeoutSeconds);

        for (const auto& [name, value] : headers)
        {
            [request setValue:toNsString(value) forHTTPHeaderField:toNsString(name)];
        }

        if (!body.empty())
        {
            request.HTTPBody = [NSData dataWithBytes:body.data() length:body.size()];
        }

        return request;
    }

    // The url loading system decodes a compressed body before handing it over, so the headers describing the encoded form would contradict what the caller receives and are dropped.
    static bool describesEncodedBody(const std::string& name)
    {
        std::string lowered;
        lowered.reserve(name.size());
        for (const char character : name)
        {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        }

        return lowered == "content-encoding" || lowered == "content-length";
    }

    static varn::http::client::ResponseHeaders collectHeaders(NSHTTPURLResponse* response)
    {
        varn::http::client::ResponseHeaders out;
        for (NSString* name in response.allHeaderFields)
        {
            id value = response.allHeaderFields[name];
            if ([value isKindOfClass:[NSString class]] && !describesEncodedBody(toStdString(name)))
            {
                out.emplace_back(toStdString(name), toStdString(value));
            }
        }

        return out;
    }
};

} // namespace

// One delegate drives both the buffered and the streaming call, since they differ only in what they do with a chunk.
@interface VarnHttpDelegate : NSObject <NSURLSessionDataDelegate>
@property(nonatomic, assign) BOOL verifyTls;
@property(nonatomic, assign) size_t maxResponseBytes;
@property(nonatomic, assign) size_t receivedBytes;
@property(nonatomic, assign) BOOL overflowed;
@property(nonatomic, strong) NSError* failure;
@property(nonatomic, strong) NSString* handlerFailure;
@property(nonatomic, assign) int status;
@property(nonatomic, strong) NSMutableData* buffer;
@property(nonatomic, strong) dispatch_semaphore_t done;
@property(nonatomic, assign) const varn::http::client::StreamResponseFn* onResponse;
@property(nonatomic, assign) const varn::http::client::StreamChunkFn* onChunk;
@property(nonatomic, assign) varn::http::client::ResponseHeaders* headers;
@end

@implementation VarnHttpDelegate

// An untrusted certificate is accepted only when the caller opted out of verification for a development server.
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

// The url loading system follows a redirect on its own, while this client hands the 3xx to the caller like every other transport does.
- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler
{
    completionHandler(nil);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler
{
    NSHTTPURLResponse* http = (NSHTTPURLResponse*)response;
    self.status = static_cast<int>(http.statusCode);
    *self.headers = AppleHttpHelpers::collectHeaders(http);

    if (self.onResponse != nullptr && *self.onResponse && ![self runHandler:^{
            (*self.onResponse)(self.status, *self.headers);
        }])
    {
        completionHandler(NSURLSessionResponseCancel);
        return;
    }

    completionHandler(NSURLSessionResponseAllow);
}

// A handler is caller code, and letting it unwind through the url loading system's queue would terminate the process.
- (BOOL)runHandler:(void (^)())work
{
    try
    {
        work();
        return YES;
    }
    catch (const std::exception& failure)
    {
        self.handlerFailure = AppleHttpHelpers::toNsString(failure.what());
    }
    catch (...)
    {
        self.handlerFailure = @"[AppleHttpClient] A response handler failed.";
    }

    return NO;
}

// The cap is enforced here rather than after the fact, so an endless producer cannot exhaust memory.
- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask didReceiveData:(NSData*)data
{
    self.receivedBytes += data.length;
    if (self.maxResponseBytes > 0 && self.receivedBytes > self.maxResponseBytes)
    {
        self.overflowed = YES;
        [dataTask cancel];
        return;
    }

    if (self.onChunk != nullptr && *self.onChunk)
    {
        if (![self runHandler:^{
                (*self.onChunk)(static_cast<const char*>(data.bytes), data.length);
            }])
        {
            [dataTask cancel];
        }

        return;
    }

    [self.buffer appendData:data];
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error
{
    self.failure = error;
    dispatch_semaphore_signal(self.done);
}

@end

namespace varn::http::client
{

namespace
{

class AppleHttpRunner
{
public:
    // The engine calls this on a pool thread and expects it to return finished, so the async session is awaited here.
    static void run(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const ClientRequestOptions& options,
        const StreamResponseFn* onResponse,
        const StreamChunkFn* onChunk,
        ClientResponse& out)
    {
        @autoreleasepool
        {
            NSMutableURLRequest* request = AppleHttpHelpers::buildRequest(method, url, headers, body, options);

            VarnHttpDelegate* delegate = [[VarnHttpDelegate alloc] init];
            delegate.verifyTls = options.verifyTls ? YES : NO;
            delegate.maxResponseBytes = options.maxResponseBytes;
            delegate.buffer = [NSMutableData data];
            delegate.done = dispatch_semaphore_create(0);
            delegate.onResponse = onResponse;
            delegate.onChunk = onChunk;
            delegate.headers = &out.headers;

            NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
            configuration.timeoutIntervalForRequest = static_cast<NSTimeInterval>(options.timeoutSeconds);
            configuration.HTTPShouldSetCookies = NO;

            // A private queue keeps the callbacks off the main thread, which this call may be blocking.
            NSOperationQueue* queue = [[NSOperationQueue alloc] init];
            queue.maxConcurrentOperationCount = 1;

            NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration delegate:delegate delegateQueue:queue];
            [[session dataTaskWithRequest:request] resume];

            dispatch_semaphore_wait(delegate.done, DISPATCH_TIME_FOREVER);
            [session finishTasksAndInvalidate];

            // The cancellation a handler or the cap triggered would otherwise be reported as a plain transport error.
            if (delegate.handlerFailure != nil)
            {
                throw std::runtime_error(AppleHttpHelpers::toStdString(delegate.handlerFailure));
            }

            if (delegate.overflowed)
            {
                throw std::runtime_error("[AppleHttpClient] The response exceeded the maximum size allowed for it.");
            }

            if (delegate.failure != nil)
            {
                throw std::runtime_error("[AppleHttpClient] " + AppleHttpHelpers::toStdString(delegate.failure.localizedDescription));
            }

            out.status = delegate.status;
            if (onChunk == nullptr)
            {
                out.body.assign(static_cast<const char*>(delegate.buffer.bytes), delegate.buffer.length);
            }
        }
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
    ClientResponse response;
    AppleHttpRunner::run(method, url, headers, body, options, nullptr, nullptr, response);
    return response;
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
    ClientResponse response;
    AppleHttpRunner::run(method, url, headers, body, options, &onResponse, &onChunk, response);
}

} // namespace varn::http::client
