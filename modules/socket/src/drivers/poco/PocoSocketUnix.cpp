#include "varn/socket/SocketTransport.h"

#include "PocoSocketStream.h"

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

void SocketTransport::connectUnixAsync(varn::runtime::Runtime& runtime, const std::string& path, ConnectCallback callback)
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
        callback(nullptr, ex.what());
        return;
    }

    socket.setBlocking(false);

    auto settled = std::make_shared<bool>(false);
    auto shared = std::make_shared<ConnectCallback>(std::move(callback));

    // clang-format off
    loop.watchWrite(socket, [&loop, socket, settled, shared]() mutable -> bool
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
        catch (...)
        {
            error = -1;
        }

        if (error != 0)
        {
            (*shared)(nullptr, "[SocketTransport] The connection could not be established.");
            return true;
        }

        (*shared)(std::make_shared<PocoStreamConnection>(socket, loop), "");
        return true;
    });
    // clang-format on
}

std::shared_ptr<TcpListener> SocketTransport::listenUnix(varn::runtime::Runtime& runtime, const std::string& path, int backlog)
{
    const Poco::Net::SocketAddress address(Poco::Net::SocketAddress::UNIX_LOCAL, path);
    Poco::Net::ServerSocket server(address, backlog);
    return std::make_shared<PocoStreamListener>(std::move(server), runtime.mainLoop(), "[PocoUnixListener] The listener is closed.");
}

} // namespace varn::socket
