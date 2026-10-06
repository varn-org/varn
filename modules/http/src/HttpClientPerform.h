#pragma once

#include "varn/http/HttpClientTypes.h"

#include <functional>
#include <memory>
#include <string>

namespace varn::http::client
{

class HttpClientConnections;
class HttpClientTransfer;

struct Hop
{
    std::string method;
    std::string url;
    Headers headers;
    bool withBody = true;
};

using HeadFn = std::function<bool(ResponseHead& head)>;

#if defined(__EMSCRIPTEN__) && defined(VARN_HTTP_CLIENT_DRIVER_EMSCRIPTEN_FETCH) && VARN_HTTP_CLIENT_DRIVER_EMSCRIPTEN_FETCH
#define VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC 1
#else
#define VARN_HTTP_CLIENT_EMSCRIPTEN_FETCH_ASYNC 0
#endif

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
