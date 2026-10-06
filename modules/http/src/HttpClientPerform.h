#pragma once

#include "varn/http/HttpClientTypes.h"

#include <functional>
#include <memory>
#include <string>

namespace varn::http::client
{

class HttpClientConnections;
class HttpClientTransfer;

/// One request of the chain a redirect makes, whose body is the one of the transfer when it carries one.
struct Hop
{
    std::string method;
    std::string url;
    Headers headers;
    bool withBody = true;
};

/// Receives the head of a response and answers whether its body is wanted.
using HeadFn = std::function<bool(ResponseHead& head)>;

#if defined(__EMSCRIPTEN__) && defined(VARN_HTTP_CLIENT_DRIVER_EMSCRIPTEN_FETCH) && VARN_HTTP_CLIENT_DRIVER_EMSCRIPTEN_FETCH
#define VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC 1
#else
#define VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC 0
#endif

/// The seam every transport driver implements, each one in its own source.
class HttpClientPerform
{
public:
    HttpClientPerform() = delete;

    static std::shared_ptr<HttpClientConnections> connections();

#if VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC
    static void start(HttpClientConnections& connections, const std::shared_ptr<HttpClientTransfer>& transfer);
#else
    static void perform(HttpClientConnections& connections, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead);
#endif
};

} // namespace varn::http::client
