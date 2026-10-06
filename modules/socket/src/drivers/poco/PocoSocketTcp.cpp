#include "varn/socket/SocketTransport.h"

#include "PocoSocketResolver.h"
#include "PocoSocketStream.h"
#include "PocoSocketText.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"

#include <Poco/Net/DNS.h>
#include <Poco/Net/HostEntry.h>
#include <Poco/Net/IPAddress.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace varn::socket
{

using varn::runtime::EventLoop;

namespace
{

// One connect from the lookup of its host to the settled socket, raced by its deadline when it has one.
class TcpConnectAttempt : public std::enable_shared_from_this<TcpConnectAttempt>
{
public:
    TcpConnectAttempt(EventLoop& loop, const std::string& host, int port, std::size_t maxQueuedBytes, ConnectCallback callback)
        : loop(loop)
        , maxQueuedBytes(maxQueuedBytes)
        , target(PocoSocketText::target(host, port))
        , callback(std::move(callback))
    {
    }

    // The deadline covers the lookup as well as the connect, so a slow resolver cannot hold the connect past it.
    void start(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs)
    {
        auto self = shared_from_this();
        if (timeoutMs > 0)
        {
            // clang-format off
            deadline = loop.postDelayed(timeoutMs, [self]
            {
                self->expire();
            });
            // clang-format on
        }

        // clang-format off
        PocoSocketResolver::resolveAsync(runtime, host, port, [self](std::optional<Poco::Net::SocketAddress> address, const std::string& error)
        {
            if (self->settled)
            {
                return;
            }

            if (!address)
            {
                self->settle(nullptr, error);
                return;
            }

            self->connect(*address);
        });
        // clang-format on
    }

private:
    void connect(const Poco::Net::SocketAddress& address)
    {
        target = address.toString();

        Poco::Net::StreamSocket attempt;
        try
        {
            attempt.connectNB(address);
        }
        catch (const std::exception& ex)
        {
            settle(nullptr, PocoSocketText::connectFailure(target, ex));
            return;
        }

        socket = attempt;
        auto self = shared_from_this();
        // clang-format off
        loop.watchWrite(attempt, [self]() -> bool
        {
            self->complete();
            return true;
        });
        // clang-format on
    }

    void complete()
    {
        if (settled)
        {
            return;
        }

        int error = 0;
        try
        {
            error = socket->impl()->socketError();
        }
        catch (const std::exception& ex)
        {
            settle(nullptr, PocoSocketText::connectFailure(target, ex));
            return;
        }

        if (error != 0)
        {
            settle(nullptr, PocoSocketText::connectFailure(target, error));
            return;
        }

        settle(std::make_shared<PocoStreamConnection>(*socket, loop, maxQueuedBytes), "");
    }

    void expire()
    {
        if (settled)
        {
            return;
        }

        settled = true;
        deadline = 0;

        if (socket)
        {
            loop.closeSocket(*socket);
        }

        callback(nullptr, "The connection to " + target + " timed out.");
    }

    void settle(std::shared_ptr<TcpConnection> connection, const std::string& error)
    {
        settled = true;
        if (deadline != 0)
        {
            loop.cancelTimer(deadline);
            deadline = 0;
        }

        callback(std::move(connection), error);
    }

    EventLoop& loop;
    std::size_t maxQueuedBytes;
    std::string target;
    ConnectCallback callback;
    std::optional<Poco::Net::StreamSocket> socket;
    EventLoop::TimerId deadline = 0;
    bool settled = false;
};

class PocoTcpListen
{
public:
    // Windows lets another socket take over a port bound with `SO_REUSEADDR`, so a listener binds there with `SO_EXCLUSIVEADDRUSE`, which Poco sets when reuse is off.
#if defined(_WIN32)
    static constexpr bool kReuseAddress = false;
#else
    static constexpr bool kReuseAddress = true;
#endif
};

} // namespace

void SocketTransport::connectAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs, std::size_t maxQueuedBytes, ConnectCallback callback)
{
    auto attempt = std::make_shared<TcpConnectAttempt>(runtime.mainLoop(), host, port, maxQueuedBytes, std::move(callback));
    attempt->start(runtime, host, port, timeoutMs);
}

// Binds without `SO_REUSEPORT` unless asked, so a second listener on a busy port fails instead of sharing it, while `SO_REUSEADDR` on POSIX still lets a restarted program bind a port in `TIME_WAIT`.
void SocketTransport::listenAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, const TcpListenOptions& options, ListenCallback callback)
{
#if defined(_WIN32)
    if (options.reusePort)
    {
        callback(nullptr, "The option \"reusePort\" is not available on Windows.");
        return;
    }
#endif

    EventLoop* loop = &runtime.mainLoop();
    // clang-format off
    PocoSocketResolver::resolveAsync(runtime, host, port, [loop, options, callback = std::move(callback)](std::optional<Poco::Net::SocketAddress> address, const std::string& error)
    {
        if (!address)
        {
            callback(nullptr, error);
            return;
        }

        std::shared_ptr<TcpListener> listener;
        try
        {
            Poco::Net::ServerSocket server;
            server.bind(*address, PocoTcpListen::kReuseAddress, options.reusePort);
            server.listen(options.backlog);
            listener = std::make_shared<PocoStreamListener>(std::move(server), *loop, options.maxQueuedBytes);
        }
        catch (const std::exception& ex)
        {
            callback(nullptr, PocoSocketText::bindFailure(address->toString(), ex));
            return;
        }

        callback(std::move(listener), "");
    });
    // clang-format on
}

// Answers a numeric address as it is and looks a name up on the I/O pool, answering every address it has once, in the order of the resolver of the system.
void SocketTransport::resolveAsync(varn::runtime::Runtime& runtime, const std::string& host, ResolveCallback callback)
{
    Poco::Net::IPAddress numeric;
    if (Poco::Net::IPAddress::tryParse(host, numeric))
    {
        callback(true, {numeric.toString()}, "");
        return;
    }

    EventLoop* loop = &runtime.mainLoop();
    // clang-format off
    runtime.ioPool().post([loop, host, callback = std::move(callback)]() mutable
    {
        std::vector<std::string> addresses;
        std::string error;
        try
        {
            const Poco::Net::HostEntry entry = Poco::Net::DNS::hostByName(host);
            for (const auto& address : entry.addresses())
            {
                addresses.push_back(address.toString());
            }
        }
        catch (const std::exception& ex)
        {
            error = PocoSocketText::resolveFailure(host, ex);
        }

        loop->post([addresses = std::move(addresses), error = std::move(error), callback = std::move(callback)]() mutable
        {
            callback(error.empty(), std::move(addresses), error);
        });
    });
    // clang-format on
}

} // namespace varn::socket
