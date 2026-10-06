#include "varn/socket/SocketTransport.h"

#include "PocoSocketStream.h"
#include "PocoSocketText.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>

#include <exception>
#include <memory>
#include <string>

namespace varn::socket
{

using varn::runtime::EventLoop;

void SocketTransport::connectUnixAsync(varn::runtime::Runtime& runtime, const std::string& path, std::size_t maxQueuedBytes, ConnectCallback callback)
{
    EventLoop& loop = runtime.mainLoop();

    Poco::Net::StreamSocket socket;
    try
    {
        const Poco::Net::SocketAddress address(Poco::Net::SocketAddress::UNIX_LOCAL, path);
        socket.connectNB(address);
    }
    catch (const std::exception& ex)
    {
        callback(nullptr, PocoSocketText::connectFailure(path, ex));
        return;
    }

    socket.setBlocking(false);

    auto settled = std::make_shared<bool>(false);
    auto shared = std::make_shared<ConnectCallback>(std::move(callback));

    // clang-format off
    loop.watchWrite(socket, [&loop, socket, path, maxQueuedBytes, settled, shared]() mutable -> bool
    {
        if (*settled)
        {
            return true;
        }

        *settled = true;

        int error = 0;
        try
        {
            error = socket.impl()->socketError();
        }
        catch (const std::exception& ex)
        {
            (*shared)(nullptr, PocoSocketText::connectFailure(path, ex));
            return true;
        }

        if (error != 0)
        {
            (*shared)(nullptr, PocoSocketText::connectFailure(path, error));
            return true;
        }

        (*shared)(std::make_shared<PocoStreamConnection>(socket, loop, maxQueuedBytes), "");
        return true;
    });
    // clang-format on
}

void SocketTransport::listenUnixAsync(varn::runtime::Runtime& runtime, const std::string& path, int backlog, std::size_t maxQueuedBytes, ListenCallback callback)
{
    std::shared_ptr<TcpListener> listener;
    try
    {
        const Poco::Net::SocketAddress address(Poco::Net::SocketAddress::UNIX_LOCAL, path);
        Poco::Net::ServerSocket server(address, backlog);
        listener = std::make_shared<PocoStreamListener>(std::move(server), runtime.mainLoop(), maxQueuedBytes);
    }
    catch (const std::exception& ex)
    {
        callback(nullptr, PocoSocketText::bindFailure(path, ex));
        return;
    }

    callback(std::move(listener), "");
}

} // namespace varn::socket
