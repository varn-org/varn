#pragma once

#include <functional>
#include <memory>
#include <string>

namespace varn::runtime
{
class Runtime;
}

namespace varn::socket
{

class TcpConnection;

struct UdpDatagram
{
    std::string data;
    std::string host;
    int port = 0;
};

using ConnectCallback = std::function<void(std::shared_ptr<TcpConnection> connection, const std::string& error)>;
using AcceptCallback = std::function<void(std::shared_ptr<TcpConnection> connection, const std::string& error)>;
using ReceiveCallback = std::function<void(bool ok, const std::string& data)>;
using SendCallback = std::function<void(bool ok, const std::string& error)>;
using ReceiveFromCallback = std::function<void(bool ok, const UdpDatagram& datagram, const std::string& error)>;

class TcpConnection
{
public:
    virtual ~TcpConnection() = default;

    virtual void receiveAsync(int maxBytes, ReceiveCallback callback) = 0;
    virtual void sendAsync(std::string data, SendCallback callback) = 0;
    virtual void close() = 0;

    virtual void startTlsAsync(varn::runtime::Runtime& /*runtime*/, std::string /*host*/, bool /*verify*/, SendCallback callback)
    {
        callback(false, "[TcpConnection] TLS upgrade is not supported by this transport.");
    }
};

class TcpListener
{
public:
    virtual ~TcpListener() = default;

    virtual void acceptAsync(AcceptCallback callback) = 0;
    virtual void close() = 0;
};

class UdpSocket
{
public:
    virtual ~UdpSocket() = default;

    virtual void sendToAsync(std::string host, int port, std::string data, SendCallback callback) = 0;
    virtual void receiveFromAsync(int maxBytes, ReceiveFromCallback callback) = 0;
    virtual void close() = 0;
};

class SocketTransport
{
public:
    SocketTransport() = delete;

    static void connectAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs, ConnectCallback callback);
    static void connectTlsAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs, bool verify, ConnectCallback callback);
    static void connectUnixAsync(varn::runtime::Runtime& runtime, const std::string& path, ConnectCallback callback);
    static std::shared_ptr<TcpListener> listen(varn::runtime::Runtime& runtime, const std::string& host, int port, int backlog);
    static std::shared_ptr<TcpListener> listenUnix(varn::runtime::Runtime& runtime, const std::string& path, int backlog);
    static std::shared_ptr<UdpSocket> bindUdp(varn::runtime::Runtime& runtime, const std::string& host, int port);
};

} // namespace varn::socket
