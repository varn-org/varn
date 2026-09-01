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

void SocketTransport::connectAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, int timeoutMs, ConnectCallback callback)
{
    EventLoop& loop = runtime.mainLoop();

    Poco::Net::StreamSocket socket;
    try
    {
        socket.connectNB(Poco::Net::SocketAddress(host, static_cast<Poco::UInt16>(port)));
    }
    catch (const std::exception& ex)
    {
        callback(nullptr, ex.what());
        return;
    }

    socket.setBlocking(false);

    // the write watcher and the optional timeout race to settle the connect and the first to fire wins
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

    if (timeoutMs > 0)
    {
        // clang-format off
        loop.postDelayed(timeoutMs, [&loop, socket, settled, shared]() mutable
        {
            if (*settled)
            {
                return;
            }

            *settled = true;
            loop.closeSocket(socket);
            (*shared)(nullptr, "[SocketTransport] The connection timed out.");
        });
        // clang-format on
    }
}

std::shared_ptr<TcpListener> SocketTransport::listen(varn::runtime::Runtime& runtime, const std::string& host, int port, int backlog)
{
    Poco::Net::ServerSocket server(Poco::Net::SocketAddress(host, static_cast<Poco::UInt16>(port)), backlog);
    return std::make_shared<PocoStreamListener>(std::move(server), runtime.mainLoop(), "[PocoTcpListener] The listener is closed.");
}

} // namespace varn::socket
