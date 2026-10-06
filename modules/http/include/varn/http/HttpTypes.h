#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace varn::http
{

struct HttpRequest
{
    std::string host;
    std::string method;
    std::string path;
    std::string target;
    std::string queryString;
    std::string body;
    std::string remoteAddress;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::pair<std::string, std::string>> cookies;
    std::vector<std::pair<std::string, std::string>> query;
};

class HttpResponse
{
public:
    using WriteDone = std::function<void(std::string error)>;

    virtual ~HttpResponse() = default;
    virtual void setStatus(int statusCode) = 0;
    virtual void setHeader(const std::string& name, const std::string& value) = 0;
    virtual void addHeader(const std::string& name, const std::string& value) = 0;
    virtual void write(std::string chunk, WriteDone done) = 0;
    virtual void end(std::string body) = 0;
    virtual void fail() = 0;
    virtual bool ended() const = 0;
    virtual void sendFile(const std::string& path, std::uint64_t start, std::uint64_t length, bool headersOnly);
};

class HttpServer
{
public:
    virtual ~HttpServer() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

struct HttpServerOptions
{
    std::string host = "0.0.0.0";
    int port = 3000;
    bool reusePort = false;
    bool tls = false;
    std::string certFile;
    std::string keyFile;
    std::string publicDir;
    bool servePublic = false;
    bool directoryListing = false;
    int maxQueued = 65536;
    int keepAliveTimeoutSeconds = 30;
    long long maxRequestBodyBytes = 16 * 1024 * 1024;
    long long requestTimeoutMs = 30000;
    bool compress = true;
};

using HttpHandler = std::function<void(HttpRequest, std::shared_ptr<HttpResponse>)>;

struct WebSocketOptions
{
    std::size_t maxMessageBytes = 16 * 1024 * 1024;
    std::size_t maxQueuedBytes = 16 * 1024 * 1024;
    long long pingIntervalMs = 0;
    std::string protocol;
};

class WebSocketConnection
{
public:
    using SendDone = std::function<void(bool sent)>;

    static bool validCloseCode(int code);

    virtual ~WebSocketConnection() = default;
    virtual void accept(WebSocketOptions options) = 0;
    virtual void reject(int statusCode) = 0;
    virtual void send(std::shared_ptr<const std::string> message, bool binary, SendDone done) = 0;
    virtual void ping(std::string payload) = 0;
    virtual void close(int code, std::string reason) = 0;
    virtual std::size_t bufferedAmount() const = 0;
    virtual void onMessage(std::function<void(const std::string& message, bool binary)> handler) = 0;
    virtual void onPong(std::function<void(const std::string& payload)> handler) = 0;
    virtual void onClose(std::function<void(int code, const std::string& reason)> handler) = 0;
};

using WebSocketHandler = std::function<void(const HttpRequest&, std::shared_ptr<WebSocketConnection>)>;

} // namespace varn::http
