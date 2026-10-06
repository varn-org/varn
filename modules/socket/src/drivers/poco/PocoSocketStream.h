#pragma once

#include "varn/socket/SocketTransport.h"

#include "varn/runtime/EventLoop.h"

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/Socket.h>
#include <Poco/Net/StreamSocket.h>

#include <cstddef>
#include <memory>
#include <string>

namespace varn::socket
{

constexpr int kMaxReceiveBytes = 16 * 1024 * 1024;

class ManagedSocket
{
public:
    ManagedSocket() = delete;

    static void close(varn::runtime::EventLoop& loop, const Poco::Net::Socket& socket);
};

class SecureLayer
{
public:
    virtual ~SecureLayer() = default;

    virtual void receive(int maxBytes, ReceiveCallback callback) = 0;
    virtual void send(std::string data, SendCallback callback) = 0;
    virtual void shutdownSend(SendCallback callback) = 0;
    virtual void close() = 0;
};

class PocoStreamConnection : public TcpConnection, public std::enable_shared_from_this<PocoStreamConnection>
{
    friend class TlsStream;

public:
    PocoStreamConnection(Poco::Net::StreamSocket socket, varn::runtime::EventLoop& loop, std::size_t maxQueuedBytes);

    void receiveAsync(int maxBytes, ReceiveCallback callback) override;
    void sendAsync(std::string data, SendCallback callback) override;
    void shutdownSendAsync(SendCallback callback) override;
    void startTlsAsync(std::string host, bool verify, int timeoutMs, SendCallback callback) override;
    void setNoDelay(bool enabled) override;
    void setKeepAlive(bool enabled) override;
    std::size_t pendingBytes() const override { return closed ? 0 : queuedBytes; }
    const SocketEndpoint& localEndpoint() const override { return local; }
    const SocketEndpoint& peerEndpoint() const override { return peer; }
    void close() override;

private:
    void unqueue(std::size_t bytes);
    char* receiveSpace(int size);
    bool readOnce(int maxBytes, const ReceiveCallback& callback);
    void watchReadOnce(int maxBytes, ReceiveCallback callback);

    Poco::Net::StreamSocket socket;
    varn::runtime::EventLoop& loop;
    std::size_t maxQueuedBytes;
    std::size_t queuedBytes = 0;
    std::size_t plainReceives = 0;
    SocketEndpoint local;
    SocketEndpoint peer;
    std::string peerText;
    bool closed = false;
    bool sendShutdown = false;
    std::shared_ptr<SecureLayer> secure;
    std::unique_ptr<char[]> receiveBuffer;
    std::size_t receiveCapacity = 0;
};

class PocoStreamListener : public TcpListener, public std::enable_shared_from_this<PocoStreamListener>
{
public:
    PocoStreamListener(Poco::Net::ServerSocket server, varn::runtime::EventLoop& loop, std::size_t maxQueuedBytes);

    void acceptAsync(AcceptCallback callback) override;
    const SocketEndpoint& endpoint() const override { return bound; }
    void close() override;

private:
    Poco::Net::ServerSocket server;
    varn::runtime::EventLoop& loop;
    std::size_t maxQueuedBytes;
    SocketEndpoint bound;
    bool closed = false;
};

} // namespace varn::socket
