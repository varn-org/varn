#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace varn::runtime
{
class Runtime;
}

namespace varn::socket
{

class TcpConnection;
class TcpListener;
class UdpSocket;

struct SocketEndpoint
{
    std::string host;
    std::optional<int> port;
};

struct UdpDatagram
{
    std::string data;
    std::string host;
    int port = 0;
};

struct TcpListenOptions
{
    int backlog = 64;
    bool reusePort = false;
    std::size_t maxQueuedBytes = 0;
};

using ConnectCallback = std::function<void(std::shared_ptr<TcpConnection> connection, const std::string& error)>;
using AcceptCallback = std::function<void(std::shared_ptr<TcpConnection> connection, const std::string& error)>;
using ListenCallback = std::function<void(std::shared_ptr<TcpListener> listener, const std::string& error)>;
using BindCallback = std::function<void(std::shared_ptr<UdpSocket> socket, const std::string& error)>;
using ResolveCallback = std::function<void(bool ok, std::vector<std::string> addresses, const std::string& error)>;
using ReceiveCallback = std::function<void(bool ok, std::string data)>;
using SendCallback = std::function<void(bool ok, const std::string& error)>;
using ReceiveFromCallback = std::function<void(bool ok, const UdpDatagram& datagram, const std::string& error)>;
using ReceiveManyCallback = std::function<void(bool ok, std::vector<UdpDatagram> datagrams, const std::string& error)>;

class TcpConnection
{
public:
    virtual ~TcpConnection() = default;

    virtual void receiveAsync(int maxBytes, ReceiveCallback callback) = 0;
    virtual void sendAsync(std::string data, SendCallback callback) = 0;
    virtual void shutdownSendAsync(SendCallback callback) = 0;
    virtual void startTlsAsync(varn::runtime::Runtime& runtime, std::string host, bool verify, SendCallback callback) = 0;
    virtual void setNoDelay(bool enabled) = 0;
    virtual void setKeepAlive(bool enabled) = 0;
    virtual std::size_t pendingBytes() const = 0;
    virtual const SocketEndpoint& localEndpoint() const = 0;
    virtual const SocketEndpoint& peerEndpoint() const = 0;
    virtual void close() = 0;
};

class TcpListener
{
public:
    virtual ~TcpListener() = default;

    virtual void acceptAsync(AcceptCallback callback) = 0;
    virtual const SocketEndpoint& endpoint() const = 0;
    virtual void close() = 0;
};

class UdpSocket
{
public:
    virtual ~UdpSocket() = default;

    virtual void sendToAsync(std::string host, int port, std::string data, SendCallback callback) = 0;
    virtual void receiveFromAsync(int maxBytes, ReceiveFromCallback callback) = 0;
    virtual void sendManyAsync(std::vector<UdpDatagram> datagrams, SendCallback callback) = 0;
    virtual void receiveManyAsync(int maxDatagrams, int maxBytes, ReceiveManyCallback callback) = 0;
    virtual void connectAsync(std::string host, int port, SendCallback callback) = 0;
    virtual void sendAsync(std::string data, SendCallback callback) = 0;
    virtual void setBroadcast(bool enabled) = 0;
    virtual void joinGroup(const std::string& group) = 0;
    virtual void leaveGroup(const std::string& group) = 0;
    virtual const SocketEndpoint& endpoint() const = 0;
    virtual void close() = 0;
};

class SocketTransport
{
public:
    SocketTransport() = delete;

    static void connectAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs, std::size_t maxQueuedBytes, ConnectCallback callback);
    static void connectTlsAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs, bool verify, std::size_t maxQueuedBytes, ConnectCallback callback);
    static void connectUnixAsync(varn::runtime::Runtime& runtime, const std::string& path, std::size_t maxQueuedBytes, ConnectCallback callback);
    static void listenAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, const TcpListenOptions& options, ListenCallback callback);
    static void listenUnixAsync(varn::runtime::Runtime& runtime, const std::string& path, int backlog, std::size_t maxQueuedBytes, ListenCallback callback);
    static void bindUdpAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, BindCallback callback);
    static void resolveAsync(varn::runtime::Runtime& runtime, const std::string& host, ResolveCallback callback);
};

} // namespace varn::socket
