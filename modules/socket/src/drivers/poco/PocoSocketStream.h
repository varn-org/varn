#pragma once

#include "varn/socket/SocketTransport.h"

#include "PocoSocketText.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/Socket.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Timespan.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace varn::socket
{

constexpr int kMaxReceiveBytes = 16 * 1024 * 1024;

class ManagedSocket
{
public:
    ManagedSocket() = delete;

    static void close(varn::runtime::EventLoop& loop, const Poco::Net::Socket& socket)
    {
        if (loop.isRunning())
        {
            loop.closeSocket(socket);
            return;
        }

        // Closes the FD directly when the loop is stopped at shutdown.
        try
        {
            socket.impl()->close();
        }
        catch (...)
        {
        }
    }
};

class PocoStreamConnection : public TcpConnection, public std::enable_shared_from_this<PocoStreamConnection>
{
public:
    PocoStreamConnection(Poco::Net::StreamSocket socket, varn::runtime::EventLoop& loop, std::size_t maxQueuedBytes)
        : socket(std::move(socket))
        , loop(loop)
        , maxQueuedBytes(maxQueuedBytes)
    {
        this->socket.setBlocking(false);

        // A peer that already reset the connection has no address to report, which leaves its endpoint empty rather than failing the connection.
        try
        {
            local = PocoSocketText::endpoint(this->socket.address());
            peer = PocoSocketText::endpoint(this->socket.peerAddress());
        }
        catch (...)
        {
        }

        // A Unix-domain socket accepted by a listener has an unnamed peer, so its errors name the path of the listener instead.
        peerText = PocoSocketText::describe(peer.host.empty() ? local : peer);
    }

    void receiveAsync(int maxBytes, ReceiveCallback callback) override
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        // A TLS socket cannot be driven by the loop readiness poll, since on Windows the Schannel I/O races the `uv_poll` re-arm and drops events, so read blocking on the I/O pool and settle back on the loop.
        if (secure)
        {
            receiveSecure(maxBytes, std::move(callback));
            return;
        }

        watchReadOnce(maxBytes, std::move(callback));
    }

    // A send is refused once the bytes already queued and its own would pass `maxQueuedBytes`, so sends that nobody awaits cannot grow without a bound.
    void sendAsync(std::string data, SendCallback callback) override
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        if (sendShutdown)
        {
            callback(false, "The socket was shut down for sending.");
            return;
        }

        if (data.size() > maxQueuedBytes - std::min(queuedBytes, maxQueuedBytes))
        {
            callback(false, "The socket already queues " + std::to_string(queuedBytes) + " bytes, so a send of " + std::to_string(data.size()) + " bytes would pass its limit of " + std::to_string(maxQueuedBytes) + " bytes.");
            return;
        }

        queuedBytes += data.size();

        // Secure writes go blocking on the I/O pool for the same reason as reads, keeping Schannel off the loop readiness poll.
        if (secure)
        {
            sendSecure(std::move(data), std::move(callback));
            return;
        }

        auto self = shared_from_this();
        auto payload = std::make_shared<std::string>(std::move(data));
        auto sent = std::make_shared<std::size_t>(0);
        // clang-format off
        loop.watchWrite(socket, [self, payload, sent, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                self->unqueue(payload->size() - *sent);
                callback(false, PocoSocketText::kSocketClosed);
                return true;
            }

            try
            {
                while (*sent < payload->size())
                {
                    const std::size_t remaining = payload->size() - *sent;
                    const int chunk = static_cast<int>(std::min(remaining, static_cast<std::size_t>(INT_MAX)));
                    const int wrote = self->socket.sendBytes(payload->data() + *sent, chunk);
                    if (wrote < 0)
                    {
                        return false;
                    }

                    if (wrote == 0)
                    {
                        self->unqueue(remaining);
                        callback(false, "The connection to " + self->peerText + " was closed before all data was sent.");
                        return true;
                    }

                    *sent += static_cast<std::size_t>(wrote);
                    self->unqueue(static_cast<std::size_t>(wrote));
                }

                callback(true, std::string());
                return true;
            }
            catch (const std::exception& ex)
            {
                self->unqueue(payload->size() - *sent);
                callback(false, PocoSocketText::failure("The send to " + self->peerText + " failed", ex));
                return true;
            }
        });
        // clang-format on
    }

    // Shuts the sending side down once every send queued before it left, so the peer reads the end of the stream while this side can still receive.
    void shutdownSendAsync(SendCallback callback) override
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        sendShutdown = true;
        auto self = shared_from_this();
        if (secure)
        {
            // clang-format off
            enqueueSecure([self, callback = std::move(callback)]() mutable
            {
                self->runtime->ioPool().post([self, callback = std::move(callback)]() mutable
                {
                    std::string error;
                    try
                    {
                        self->socket.shutdownSend();
                    }
                    catch (const std::exception& ex)
                    {
                        error = PocoSocketText::failure("The shutdown of " + self->peerText + " failed", ex);
                    }

                    self->loop.post([self, error, callback = std::move(callback)]() mutable
                    {
                        self->settleSecure(error.empty(), error, callback);
                        self->completeSecure();
                    });
                });
            });
            // clang-format on
            return;
        }

        // clang-format off
        loop.watchWrite(socket, [self, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(false, PocoSocketText::kSocketClosed);
                return true;
            }

            try
            {
                self->socket.shutdownSend();
                callback(true, std::string());
            }
            catch (const std::exception& ex)
            {
                callback(false, PocoSocketText::failure("The shutdown of " + self->peerText + " failed", ex));
            }

            return true;
        });
        // clang-format on
    }

    void setNoDelay(bool enabled) override
    {
        if (closed)
        {
            throw std::runtime_error(PocoSocketText::kSocketClosed);
        }

        try
        {
            socket.setNoDelay(enabled);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error(PocoSocketText::failure("The option \"TCP_NODELAY\" could not be set on " + peerText, ex));
        }
    }

    void setKeepAlive(bool enabled) override
    {
        if (closed)
        {
            throw std::runtime_error(PocoSocketText::kSocketClosed);
        }

        try
        {
            socket.setKeepAlive(enabled);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error(PocoSocketText::failure("The option \"SO_KEEPALIVE\" could not be set on " + peerText, ex));
        }
    }

    // A closed socket queues nothing, even while its pending sends are still being released.
    std::size_t pendingBytes() const override { return closed ? 0 : queuedBytes; }

    const SocketEndpoint& localEndpoint() const override { return local; }

    const SocketEndpoint& peerEndpoint() const override { return peer; }

    void close() override
    {
        closed = true;

        // A secure connection drives blocking I/O on the pool, so shut the transport down to unblock any in-flight syscall without freeing the FD, which RAII then closes once the last pool task drops this connection.
        if (secure)
        {
            const auto fd = socket.impl()->sockfd();
            if (fd != POCO_INVALID_SOCKET)
            {
#if defined(_WIN32)
                ::shutdown(fd, SD_BOTH);
#else
                ::shutdown(fd, SHUT_RDWR);
#endif
            }

            return;
        }

        ManagedSocket::close(loop, socket);
    }

    void startTlsAsync(varn::runtime::Runtime& runtime, std::string host, bool verify, SendCallback callback) override;

    // Swaps the plaintext transport for the handshaked secure socket so later reads and writes run blocking over TLS.
    void adoptSecure(const Poco::Net::StreamSocket& upgraded, varn::runtime::Runtime& rt)
    {
        socket = upgraded;
        enterSecure(rt);
    }

    // Marks a connection that was already established over TLS so its I/O runs blocking on the pool.
    void markSecure(varn::runtime::Runtime& rt) { enterSecure(rt); }

    // Runs the next queued secure operation once the current one settles, called on the loop thread.
    void completeSecure()
    {
        secureBusy = false;
        pumpSecure();
    }

    // Enqueues a secure operation so TLS reads, writes and the handshake never touch the same SSL object concurrently on the I/O pool.
    void enqueueSecure(std::function<void()> op)
    {
        secureQueue.push_back(std::move(op));
        pumpSecure();
    }

private:
    void pumpSecure()
    {
        if (secureBusy || secureQueue.empty())
        {
            return;
        }

        secureBusy = true;
        auto op = std::move(secureQueue.front());
        secureQueue.pop_front();
        op();
    }

    void enterSecure(varn::runtime::Runtime& rt)
    {
        secure = true;
        runtime = &rt;
        socket.setBlocking(true);
    }

    void unqueue(std::size_t bytes) { queuedBytes -= std::min(bytes, queuedBytes); }

    // Settles a secure operation on the loop thread, where a close that came first wins over what the pool answered.
    void settleSecure(bool ok, const std::string& error, const SendCallback& callback) const
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        callback(ok, ok ? std::string() : error);
    }

    // Reads one chunk blocking on the I/O pool and settles the callback back on the loop thread.
    void receiveSecure(int maxBytes, ReceiveCallback callback)
    {
        auto self = shared_from_this();
        // clang-format off
        enqueueSecure([self, maxBytes, callback = std::move(callback)]() mutable
        {
            self->runtime->ioPool().post([self, maxBytes, callback = std::move(callback)]() mutable
            {
                bool ok = false;
                std::string data;
                try
                {
                    // The strand runs one secure operation at a time, so the receive buffer of the connection is free here.
                    const int capped = maxBytes < kMaxReceiveBytes ? maxBytes : kMaxReceiveBytes;
                    const int received = self->socket.receiveBytes(self->receiveSpace(capped), capped);
                    ok = true;
                    if (received > 0)
                    {
                        data.assign(self->receiveBuffer.get(), static_cast<std::size_t>(received));
                    }
                }
                catch (const std::exception& ex)
                {
                    data = PocoSocketText::failure("The receive from " + self->peerText + " failed", ex);
                }

                self->loop.post([self, ok, data = std::move(data), callback = std::move(callback)]() mutable
                {
                    if (self->closed)
                    {
                        callback(false, PocoSocketText::kSocketClosed);
                    }
                    else
                    {
                        callback(ok, std::move(data));
                    }

                    self->completeSecure();
                });
            });
        });
        // clang-format on
    }

    // Writes the whole payload blocking on the I/O pool and settles the callback back on the loop thread.
    void sendSecure(std::string data, SendCallback callback)
    {
        auto self = shared_from_this();
        auto payload = std::make_shared<std::string>(std::move(data));
        // clang-format off
        enqueueSecure([self, payload, callback = std::move(callback)]() mutable
        {
            self->runtime->ioPool().post([self, payload, callback = std::move(callback)]() mutable
            {
                std::string error;
                try
                {
                    std::size_t sent = 0;
                    while (sent < payload->size())
                    {
                        const std::size_t remaining = payload->size() - sent;
                        const int chunk = static_cast<int>(std::min(remaining, static_cast<std::size_t>(INT_MAX)));
                        const int wrote = self->socket.sendBytes(payload->data() + sent, chunk);
                        if (wrote <= 0)
                        {
                            throw std::runtime_error("The connection was closed before all data was sent");
                        }

                        sent += static_cast<std::size_t>(wrote);
                    }
                }
                catch (const std::exception& ex)
                {
                    error = PocoSocketText::failure("The send to " + self->peerText + " failed", ex);
                }

                self->loop.post([self, payload, error, callback = std::move(callback)]() mutable
                {
                    self->unqueue(payload->size());
                    self->settleSecure(error.empty(), error, callback);
                    self->completeSecure();
                });
            });
        });
        // clang-format on
    }

    // Answers a buffer of at least the size asked for, kept by the connection for every receive and grown only when a larger one is asked, without filling it.
    char* receiveSpace(int size)
    {
        const auto wanted = static_cast<std::size_t>(size);
        if (wanted > receiveCapacity)
        {
            receiveBuffer = std::make_unique_for_overwrite<char[]>(wanted);
            receiveCapacity = wanted;
        }

        return receiveBuffer.get();
    }

    // Performs a single read and returns whether the callback was invoked, `false` meaning the read would block.
    bool readOnce(int maxBytes, const ReceiveCallback& callback)
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return true;
        }

        try
        {
            const int capped = maxBytes < kMaxReceiveBytes ? maxBytes : kMaxReceiveBytes;
            const int received = socket.receiveBytes(receiveSpace(capped), capped);
            if (received < 0)
            {
                return false;
            }

            callback(true, std::string(receiveBuffer.get(), static_cast<std::size_t>(received)));
            return true;
        }
        catch (const std::exception& ex)
        {
            callback(false, PocoSocketText::failure("The receive from " + peerText + " failed", ex));
            return true;
        }
    }

    void watchReadOnce(int maxBytes, ReceiveCallback callback)
    {
        auto self = shared_from_this();
        // clang-format off
        loop.watchRead(socket, [self, maxBytes, callback = std::move(callback)]() -> bool
        {
            return self->readOnce(maxBytes, callback);
        });
        // clang-format on
    }

    Poco::Net::StreamSocket socket;
    varn::runtime::EventLoop& loop;
    varn::runtime::Runtime* runtime = nullptr;
    std::size_t maxQueuedBytes;
    std::size_t queuedBytes = 0;
    SocketEndpoint local;
    SocketEndpoint peer;
    std::string peerText;
    bool closed = false;
    bool sendShutdown = false;
    bool secure = false;
    std::unique_ptr<char[]> receiveBuffer;
    std::size_t receiveCapacity = 0;
    std::deque<std::function<void()>> secureQueue;
    bool secureBusy = false;
};

class PocoStreamListener : public TcpListener, public std::enable_shared_from_this<PocoStreamListener>
{
public:
    PocoStreamListener(Poco::Net::ServerSocket server, varn::runtime::EventLoop& loop, std::size_t maxQueuedBytes)
        : server(std::move(server))
        , loop(loop)
        , maxQueuedBytes(maxQueuedBytes)
        , bound(PocoSocketText::endpoint(this->server.address()))
    {
        this->server.setBlocking(false);
    }

    void acceptAsync(AcceptCallback callback) override
    {
        if (closed)
        {
            callback(nullptr, PocoSocketText::kListenerClosed);
            return;
        }

        auto self = shared_from_this();
        // clang-format off
        loop.watchRead(server, [self, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(nullptr, PocoSocketText::kListenerClosed);
                return true;
            }

            try
            {
                // An earlier accept served in the same readiness may have taken the last waiting connection, so this one waits for the next.
                if (!self->server.poll(Poco::Timespan(0), Poco::Net::Socket::SELECT_READ))
                {
                    return false;
                }

                Poco::Net::StreamSocket accepted = self->server.acceptConnection();
                callback(std::make_shared<PocoStreamConnection>(std::move(accepted), self->loop, self->maxQueuedBytes), "");
                return true;
            }
            catch (const std::exception& ex)
            {
                callback(nullptr, PocoSocketText::failure("The listener on " + PocoSocketText::describe(self->bound) + " could not accept a connection", ex));
                return true;
            }
        });
        // clang-format on
    }

    const SocketEndpoint& endpoint() const override { return bound; }

    void close() override
    {
        closed = true;
        ManagedSocket::close(loop, server);
    }

private:
    Poco::Net::ServerSocket server;
    varn::runtime::EventLoop& loop;
    std::size_t maxQueuedBytes;
    SocketEndpoint bound;
    bool closed = false;
};

} // namespace varn::socket
