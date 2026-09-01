#pragma once

#include "varn/http/HttpTypes.h"

#include <Poco/Net/ServerSocket.h>

#include <atomic>
#include <memory>

namespace varn::runtime
{
class Runtime;
}

namespace varn::http
{

class ReactorHttpServer final : public HttpServer
{
public:
    ReactorHttpServer(varn::runtime::Runtime& rt, HttpServerOptions opts, HttpHandler onRequest, WebSocketHandler onWebSocket = {});
    ~ReactorHttpServer() override;

    void start() override;
    void stop() override;

private:
    varn::runtime::Runtime& runtime;
    HttpServerOptions serverOptions;
    HttpHandler httpHandler;
    WebSocketHandler webSocketHandler;
    Poco::Net::ServerSocket listener;
    std::shared_ptr<std::atomic<bool>> stopping = std::make_shared<std::atomic<bool>>(false);
    bool started = false;
};

} // namespace varn::http
