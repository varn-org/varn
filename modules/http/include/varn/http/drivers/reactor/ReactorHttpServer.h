#pragma once

#include "varn/http/HttpTypes.h"

#include <Poco/Net/ServerSocket.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace varn::runtime
{
class Runtime;
}

namespace varn::http
{

class ReactorServerState;

class ReactorHttpServer final : public HttpServer
{
public:
    ReactorHttpServer(varn::runtime::Runtime& rt, HttpServerOptions opts, HttpHandler onRequest, WebSocketHandler onWebSocket = {});
    ~ReactorHttpServer() override;

    void start() override;
    void stop() override;
    void close(bool graceful, long long timeoutMs, std::function<void()> done);

    const std::string& host() const;
    int port() const;
    bool tls() const;

private:
    void bindListener(const Poco::Net::SocketAddress& address, int backlog);

    varn::runtime::Runtime& runtime;
    HttpServerOptions serverOptions;
    HttpHandler httpHandler;
    WebSocketHandler webSocketHandler;
    Poco::Net::ServerSocket listener;
    std::shared_ptr<ReactorServerState> state;
    int boundPort = 0;
    std::atomic<bool> started{false};
};

} // namespace varn::http
