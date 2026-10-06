#include "PocoSocketStream.h"

#include "PocoSocketText.h"

#include <Poco/Timespan.h>

#include <algorithm>
#include <climits>
#include <exception>
#include <stdexcept>
#include <utility>

namespace varn::socket
{

// Closes through the loop while it runs, so its pending handlers settle, and closes the descriptor directly once the loop stopped at shutdown.
void ManagedSocket::close(varn::runtime::EventLoop& loop, const Poco::Net::Socket& socket)
{
    if (loop.isRunning())
    {
        loop.closeSocket(socket);
        return;
    }

    try
    {
        socket.impl()->close();
    }
    catch (...)
    {
    }
}

PocoStreamConnection::PocoStreamConnection(Poco::Net::StreamSocket socket, varn::runtime::EventLoop& loop, std::size_t maxQueuedBytes)
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

void PocoStreamConnection::receiveAsync(int maxBytes, ReceiveCallback callback)
{
    if (closed)
    {
        callback(false, PocoSocketText::kSocketClosed);
        return;
    }

    if (secure)
    {
        secure->receive(maxBytes, std::move(callback));
        return;
    }

    watchReadOnce(maxBytes, std::move(callback));
}

// A send is refused once the bytes already queued and its own would pass `maxQueuedBytes`, so sends that nobody awaits cannot grow without a bound.
// Only the I/O runs inside the guard and the callback runs once after it, so a callback that fails is never called again with that failure.
void PocoStreamConnection::sendAsync(std::string data, SendCallback callback)
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

    if (secure)
    {
        secure->send(std::move(data), std::move(callback));
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

        std::string error;
        try
        {
            while (*sent < payload->size())
            {
                const int chunk = static_cast<int>(std::min(payload->size() - *sent, static_cast<std::size_t>(INT_MAX)));
                const int wrote = self->socket.sendBytes(payload->data() + *sent, chunk);
                if (wrote < 0)
                {
                    return false;
                }

                if (wrote == 0)
                {
                    error = "The connection to " + self->peerText + " was closed before all data was sent.";
                    break;
                }

                *sent += static_cast<std::size_t>(wrote);
                self->unqueue(static_cast<std::size_t>(wrote));
            }
        }
        catch (const std::exception& ex)
        {
            error = PocoSocketText::failure("The send to " + self->peerText + " failed", ex);
        }

        self->unqueue(payload->size() - *sent);
        callback(error.empty(), error);
        return true;
    });
    // clang-format on
}

// Shuts the sending side down once every send queued before it left, so the peer reads the end of the stream while this side can still receive.
void PocoStreamConnection::shutdownSendAsync(SendCallback callback)
{
    if (closed)
    {
        callback(false, PocoSocketText::kSocketClosed);
        return;
    }

    sendShutdown = true;
    if (secure)
    {
        secure->shutdownSend(std::move(callback));
        return;
    }

    auto self = shared_from_this();
    // clang-format off
    loop.watchWrite(socket, [self, callback = std::move(callback)]() -> bool
    {
        if (self->closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return true;
        }

        std::string error;
        try
        {
            self->socket.shutdownSend();
        }
        catch (const std::exception& ex)
        {
            error = PocoSocketText::failure("The shutdown of " + self->peerText + " failed", ex);
        }

        callback(error.empty(), error);
        return true;
    });
    // clang-format on
}

void PocoStreamConnection::setNoDelay(bool enabled)
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

void PocoStreamConnection::setKeepAlive(bool enabled)
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

void PocoStreamConnection::close()
{
    if (closed)
    {
        return;
    }

    closed = true;
    if (secure)
    {
        secure->close();
    }

    ManagedSocket::close(loop, socket);
}

void PocoStreamConnection::unqueue(std::size_t bytes)
{
    queuedBytes -= std::min(bytes, queuedBytes);
}

// Answers a buffer of at least the size asked for, kept by the connection for every receive and grown only when a larger one is asked, without filling it.
char* PocoStreamConnection::receiveSpace(int size)
{
    const auto wanted = static_cast<std::size_t>(size);
    if (wanted > receiveCapacity)
    {
        receiveBuffer = std::make_unique_for_overwrite<char[]>(wanted);
        receiveCapacity = wanted;
    }

    return receiveBuffer.get();
}

// Performs a single read of the plain stream and answers whether the receive settled, false meaning the read would block, and runs the callback once outside the guard of the read.
bool PocoStreamConnection::readOnce(int maxBytes, const ReceiveCallback& callback)
{
    bool ok = false;
    std::string data = PocoSocketText::kSocketClosed;
    if (!closed)
    {
        const int capped = maxBytes < kMaxReceiveBytes ? maxBytes : kMaxReceiveBytes;
        try
        {
            const int received = socket.receiveBytes(receiveSpace(capped), capped);
            if (received < 0)
            {
                return false;
            }

            ok = true;
            data.assign(receiveBuffer.get(), static_cast<std::size_t>(received));
        }
        catch (const std::exception& ex)
        {
            data = PocoSocketText::failure("The receive from " + peerText + " failed", ex);
        }
    }

    --plainReceives;
    callback(ok, std::move(data));
    return true;
}

// Counts the receive while it waits, since a receive of the plain stream would take the bytes that a TLS handshake started next waits for.
void PocoStreamConnection::watchReadOnce(int maxBytes, ReceiveCallback callback)
{
    auto self = shared_from_this();
    ++plainReceives;
    // clang-format off
    loop.watchRead(socket, [self, maxBytes, callback = std::move(callback)]() -> bool
    {
        return self->readOnce(maxBytes, callback);
    });
    // clang-format on
}

PocoStreamListener::PocoStreamListener(Poco::Net::ServerSocket server, varn::runtime::EventLoop& loop, std::size_t maxQueuedBytes)
    : server(std::move(server))
    , loop(loop)
    , maxQueuedBytes(maxQueuedBytes)
    , bound(PocoSocketText::endpoint(this->server.address()))
{
    this->server.setBlocking(false);
}

// Accepts inside the guard and runs the callback once after it, so a callback that fails is never called again with that failure.
void PocoStreamListener::acceptAsync(AcceptCallback callback)
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

        std::shared_ptr<TcpConnection> connection;
        std::string error;
        try
        {
            // An earlier accept served in the same readiness may have taken the last waiting connection, so this one waits for the next.
            if (!self->server.poll(Poco::Timespan(0), Poco::Net::Socket::SELECT_READ))
            {
                return false;
            }

            connection = std::make_shared<PocoStreamConnection>(self->server.acceptConnection(), self->loop, self->maxQueuedBytes);
        }
        catch (const std::exception& ex)
        {
            error = PocoSocketText::failure("The listener on " + PocoSocketText::describe(self->bound) + " could not accept a connection", ex);
        }

        callback(std::move(connection), error);
        return true;
    });
    // clang-format on
}

void PocoStreamListener::close()
{
    closed = true;
    ManagedSocket::close(loop, server);
}

} // namespace varn::socket
