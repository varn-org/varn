#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace varn::async
{
class Promise;
}

namespace varn::http::client
{

struct ClientRequestOptions
{
    int timeoutSeconds = 60;
    bool verifyTls = true;
    std::size_t maxResponseBytes = 64u * 1024u * 1024u;
};

using ResponseHeaders = std::vector<std::pair<std::string, std::string>>;

struct ClientResponse
{
    int status = 0;
    ResponseHeaders headers;
    std::string body;
};

using StreamResponseFn = std::function<void(int, const ResponseHeaders&)>;
using StreamChunkFn = std::function<void(const char*, std::size_t)>;

#if defined(__EMSCRIPTEN__) && defined(VARN_HTTP_CLIENT_DRIVER_EMSCRIPTEN_FETCH) && VARN_HTTP_CLIENT_DRIVER_EMSCRIPTEN_FETCH
#define VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC 1
#else
#define VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC 0
#endif

class HttpClientPerform
{
public:
    HttpClientPerform() = delete;

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
    static void performAsync(
        const std::shared_ptr<varn::async::Promise>& promise,
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        int timeoutSeconds,
        std::size_t maxResponseBytes);
#else
    static ClientResponse perform(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const ClientRequestOptions& options);

    static void performStream(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const ClientRequestOptions& options,
        const StreamResponseFn& onResponse,
        const StreamChunkFn& onChunk);
#endif
};

} // namespace varn::http::client
