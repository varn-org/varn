#include "varn/socket/SocketTransport.h"

#include "PocoSocketStream.h"
#include "PocoSocketText.h"
#include "TlsSession.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace varn::socket
{

using varn::runtime::EventLoop;

// Runs TLS on the loop thread over the plain socket the loop already watches, so a receive and a send are in flight at once and no thread waits on the peer.
class TlsStream final : public SecureLayer
{
public:
    TlsStream(PocoStreamConnection& owner, std::string host, bool verify)
        : owner(owner)
        , host(std::move(host))
        , session(this->host, verify)
    {
    }

    void start(SendCallback callback)
    {
        handshakeDone = std::move(callback);
        advanceHandshake();
    }

    void receive(int maxBytes, ReceiveCallback callback) override
    {
        if (!failure.empty())
        {
            callback(false, failure);
            return;
        }

        if (ended)
        {
            callback(true, std::string());
            return;
        }

        receives.push_back({maxBytes, std::move(callback)});

        if (established)
        {
            serveReceives();
        }
    }

    void send(std::string data, SendCallback callback) override
    {
        if (!failure.empty())
        {
            owner.unqueue(data.size());
            callback(false, failure);
            return;
        }

        if (!established)
        {
            held.push_back({false, std::move(data), std::move(callback)});
            return;
        }

        encrypt(std::move(data), std::move(callback));
    }

    void shutdownSend(SendCallback callback) override
    {
        if (!failure.empty())
        {
            callback(false, failure);
            return;
        }

        if (!established)
        {
            held.push_back({true, std::string(), std::move(callback)});
            return;
        }

        endSession(std::move(callback));
    }

    // A close waits for nothing, so the end of the session goes out only as far as the socket takes it at once.
    void close() override
    {
        if (!established || !failure.empty() || closeNotified)
        {
            return;
        }

        closeNotified = true;
        session.closeNotify();
        collect();

        try
        {
            while (cipherOffset < cipher.size())
            {
                const int wrote = owner.socket.sendBytes(cipher.data() + cipherOffset, chunkAt(cipherOffset));
                if (wrote <= 0)
                {
                    return;
                }

                cipherOffset += static_cast<std::size_t>(wrote);
            }
        }
        catch (const std::exception&)
        {
        }
    }

private:
    struct PendingReceive
    {
        int maxBytes;
        ReceiveCallback callback;
    };

    struct HeldSend
    {
        bool shutdown;
        std::string data;
        SendCallback callback;
    };

    struct InflightSend
    {
        std::uint64_t end;
        std::size_t plainBytes;
        bool shutdown;
        SendCallback callback;
    };

    static constexpr std::size_t kRawReadBytes = 64 * 1024;
    static constexpr std::size_t kCompactBytes = 64 * 1024;

    void advanceHandshake()
    {
        const TlsSession::Status status = session.handshake();
        collect();
        flush();

        if (status == TlsSession::Status::NeedsInput)
        {
            readRaw();
            return;
        }

        if (status != TlsSession::Status::Ready)
        {
            const std::string reason = status == TlsSession::Status::Closed ? "the peer closed the connection" : session.failure();
            failAll(PocoSocketText::failure("The TLS handshake with " + host + " failed", std::runtime_error(reason)));
            return;
        }

        established = true;
        auto done = std::move(handshakeDone);
        handshakeDone = nullptr;
        done(true, std::string());

        std::deque<HeldSend> waiting = std::move(held);
        held.clear();
        for (auto& pending : waiting)
        {
            if (pending.shutdown)
            {
                shutdownSend(std::move(pending.callback));
                continue;
            }

            send(std::move(pending.data), std::move(pending.callback));
        }

        serveReceives();
    }

    void serveReceives()
    {
        while (!receives.empty() && failure.empty())
        {
            const int capped = std::min(receives.front().maxBytes, kMaxReceiveBytes);
            char* space = owner.receiveSpace(capped);
            const TlsSession::ReadResult result = session.read(space, static_cast<std::size_t>(capped));
            collect();
            flush();

            if (result.status == TlsSession::Status::NeedsInput)
            {
                readRaw();
                return;
            }

            if (result.status == TlsSession::Status::Failed)
            {
                failAll(PocoSocketText::failure("The receive from " + owner.peerText + " failed", std::runtime_error(session.failure())));
                return;
            }

            PendingReceive served = std::move(receives.front());
            receives.pop_front();

            if (result.status == TlsSession::Status::Closed)
            {
                ended = true;
                served.callback(true, std::string());
                continue;
            }

            served.callback(true, std::string(space, result.bytes));
        }
    }

    void encrypt(std::string data, SendCallback callback)
    {
        if (!session.write(data))
        {
            owner.unqueue(data.size());
            const std::string error = PocoSocketText::failure("The send to " + owner.peerText + " failed", std::runtime_error(session.failure()));
            callback(false, error);
            failAll(error);
            return;
        }

        collect();
        inflight.push_back({produced, data.size(), false, std::move(callback)});
        flush();
        settleFlushed();
    }

    void endSession(SendCallback callback)
    {
        session.closeNotify();
        closeNotified = true;
        collect();
        inflight.push_back({produced, 0, true, std::move(callback)});
        flush();
        settleFlushed();
    }

    void collect()
    {
        if (session.pendingCipher() == 0)
        {
            return;
        }

        const std::string taken = session.takeCipher();
        if (cipherOffset >= kCompactBytes)
        {
            cipher.erase(0, cipherOffset);
            cipherOffset = 0;
        }

        cipher.append(taken);
        produced += taken.size();
    }

    int chunkAt(std::size_t offset) const
    {
        return static_cast<int>(std::min(cipher.size() - offset, static_cast<std::size_t>(INT_MAX)));
    }

    void readRaw()
    {
        if (reading)
        {
            return;
        }

        reading = true;
        auto keep = owner.shared_from_this();
        // clang-format off
        owner.loop.watchRead(owner.socket, [this, keep]() -> bool
        {
            return onReadable();
        });
        // clang-format on
    }

    bool onReadable()
    {
        if (owner.closed)
        {
            reading = false;
            failAll(PocoSocketText::kSocketClosed);
            return true;
        }

        if (!rawBuffer)
        {
            rawBuffer = std::make_unique_for_overwrite<char[]>(kRawReadBytes);
        }

        try
        {
            const int received = owner.socket.receiveBytes(rawBuffer.get(), static_cast<int>(kRawReadBytes));
            if (received < 0)
            {
                return false;
            }

            if (received == 0)
            {
                session.receiveEnd();
            }
            else
            {
                session.receiveCipher(rawBuffer.get(), static_cast<std::size_t>(received));
            }
        }
        catch (const std::exception& ex)
        {
            reading = false;
            failAll(PocoSocketText::failure(established ? "The receive from " + owner.peerText + " failed" : "The TLS handshake with " + host + " failed", ex));
            return true;
        }

        reading = false;

        if (established)
        {
            serveReceives();
            return true;
        }

        advanceHandshake();
        return true;
    }

    void flush()
    {
        if (writing || cipherOffset == cipher.size())
        {
            return;
        }

        writing = true;
        auto keep = owner.shared_from_this();
        // clang-format off
        owner.loop.watchWrite(owner.socket, [this, keep]() -> bool
        {
            return onWritable();
        });
        // clang-format on
    }

    bool onWritable()
    {
        if (owner.closed)
        {
            writing = false;
            failAll(PocoSocketText::kSocketClosed);
            return true;
        }

        try
        {
            while (cipherOffset < cipher.size())
            {
                const int wrote = owner.socket.sendBytes(cipher.data() + cipherOffset, chunkAt(cipherOffset));
                if (wrote < 0)
                {
                    settleFlushed();
                    return false;
                }

                if (wrote == 0)
                {
                    writing = false;
                    failAll("The connection to " + owner.peerText + " was closed before all data was sent.");
                    return true;
                }

                cipherOffset += static_cast<std::size_t>(wrote);
                flushed += static_cast<std::uint64_t>(wrote);
            }
        }
        catch (const std::exception& ex)
        {
            writing = false;
            failAll(PocoSocketText::failure("The send to " + owner.peerText + " failed", ex));
            return true;
        }

        cipher.clear();
        cipherOffset = 0;
        writing = false;
        settleFlushed();
        return true;
    }

    void settleFlushed()
    {
        while (!inflight.empty() && inflight.front().end <= flushed)
        {
            InflightSend done = std::move(inflight.front());
            inflight.pop_front();
            owner.unqueue(done.plainBytes);

            if (!done.shutdown)
            {
                done.callback(true, std::string());
                continue;
            }

            try
            {
                owner.socket.shutdownSend();
                done.callback(true, std::string());
            }
            catch (const std::exception& ex)
            {
                done.callback(false, PocoSocketText::failure("The shutdown of " + owner.peerText + " failed", ex));
            }
        }
    }

    // Every later operation answers the same failure.
    void failAll(const std::string& error)
    {
        if (failure.empty())
        {
            failure = error;
        }

        if (handshakeDone)
        {
            auto done = std::move(handshakeDone);
            handshakeDone = nullptr;
            done(false, failure);
        }

        std::deque<PendingReceive> waitingReceives = std::move(receives);
        receives.clear();
        for (auto& pending : waitingReceives)
        {
            pending.callback(false, failure);
        }

        std::deque<HeldSend> waitingHeld = std::move(held);
        held.clear();
        for (auto& pending : waitingHeld)
        {
            owner.unqueue(pending.data.size());
            pending.callback(false, failure);
        }

        std::deque<InflightSend> waitingInflight = std::move(inflight);
        inflight.clear();
        for (auto& pending : waitingInflight)
        {
            owner.unqueue(pending.plainBytes);
            pending.callback(false, failure);
        }
    }

    PocoStreamConnection& owner;
    std::string host;
    TlsSession session;
    SendCallback handshakeDone;
    std::deque<PendingReceive> receives;
    std::deque<HeldSend> held;
    std::deque<InflightSend> inflight;
    std::string cipher;
    std::size_t cipherOffset = 0;
    std::uint64_t produced = 0;
    std::uint64_t flushed = 0;
    std::unique_ptr<char[]> rawBuffer;
    std::string failure;
    bool established = false;
    bool ended = false;
    bool closeNotified = false;
    bool reading = false;
    bool writing = false;
};

void PocoStreamConnection::startTlsAsync(std::string host, bool verify, SendCallback callback)
{
    if (closed)
    {
        callback(false, PocoSocketText::kSocketClosed);
        return;
    }

    if (secure)
    {
        callback(false, "The socket already runs TLS.");
        return;
    }

    std::shared_ptr<TlsStream> stream;
    try
    {
        stream = std::make_shared<TlsStream>(*this, host, verify);
    }
    catch (const std::exception& ex)
    {
        callback(false, PocoSocketText::failure("The TLS handshake with " + host + " failed", ex));
        return;
    }

    secure = stream;
    stream->start(std::move(callback));
}

namespace
{

// The deadline covers the plain connect and the handshake together.
class TlsConnectAttempt : public std::enable_shared_from_this<TlsConnectAttempt>
{
public:
    TlsConnectAttempt(EventLoop& loop, std::string host, int timeoutMs, bool verify, ConnectCallback callback)
        : loop(loop)
        , host(std::move(host))
        , timeoutMs(timeoutMs)
        , verify(verify)
        , started(std::chrono::steady_clock::now())
        , callback(std::move(callback))
    {
    }

    void start(varn::runtime::Runtime& runtime, int port, std::size_t maxQueuedBytes)
    {
        auto self = shared_from_this();
        // clang-format off
        SocketTransport::connectAsync(runtime, host, port, timeoutMs, maxQueuedBytes, [self](std::shared_ptr<TcpConnection> connection, const std::string& error)
        {
            if (!connection)
            {
                self->callback(nullptr, error);
                return;
            }

            self->handshake(std::move(connection));
        });
        // clang-format on
    }

private:
    void handshake(std::shared_ptr<TcpConnection> opened)
    {
        connection = std::move(opened);
        auto self = shared_from_this();

        if (timeoutMs > 0)
        {
            const auto spent = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
            const long long remaining = std::max<long long>(1, timeoutMs - spent);
            // clang-format off
            deadline = loop.postDelayed(remaining, [self]
            {
                self->expire();
            });
            // clang-format on
        }

        // clang-format off
        connection->startTlsAsync(host, verify, [self](bool ok, const std::string& error)
        {
            self->settle(ok, error);
        });
        // clang-format on
    }

    void settle(bool ok, const std::string& error)
    {
        if (settled)
        {
            return;
        }

        settled = true;

        if (deadline != 0)
        {
            loop.cancelTimer(deadline);
        }

        auto established = std::move(connection);
        if (!ok)
        {
            established->close();
            callback(nullptr, error);
            return;
        }

        callback(std::move(established), std::string());
    }

    void expire()
    {
        if (settled)
        {
            return;
        }

        settled = true;
        deadline = 0;
        connection->close();
        connection.reset();
        callback(nullptr, "The TLS handshake with " + host + " did not finish within " + std::to_string(timeoutMs) + " ms.");
    }

    EventLoop& loop;
    std::string host;
    int timeoutMs;
    bool verify;
    std::chrono::steady_clock::time_point started;
    ConnectCallback callback;
    std::shared_ptr<TcpConnection> connection;
    EventLoop::TimerId deadline = 0;
    bool settled = false;
};

} // namespace

void SocketTransport::connectTlsAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs, bool verify, std::size_t maxQueuedBytes, ConnectCallback callback)
{
    auto attempt = std::make_shared<TlsConnectAttempt>(runtime.mainLoop(), host, timeoutMs, verify, std::move(callback));
    attempt->start(runtime, port, maxQueuedBytes);
}

} // namespace varn::socket
