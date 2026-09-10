#include "varn/socket/SocketTransport.h"

#include "PocoSocketStream.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"

#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/SocketAddress.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace varn::socket
{

using varn::runtime::EventLoop;

namespace
{

constexpr int kMaxDatagramBytes = 65536;

class PocoUdpSocket : public UdpSocket, public std::enable_shared_from_this<PocoUdpSocket>
{
public:
    PocoUdpSocket(Poco::Net::DatagramSocket socket, EventLoop& loop)
        : socket(std::move(socket))
        , loop(loop)
    {
        this->socket.setBlocking(false);
    }

    void sendToAsync(std::string host, int port, std::string data, SendCallback callback) override
    {
        if (closed)
        {
            callback(false, "[PocoUdpSocket] The socket is closed.");
            return;
        }

        std::shared_ptr<Poco::Net::SocketAddress> destination;
        try
        {
            destination = std::make_shared<Poco::Net::SocketAddress>(host, static_cast<Poco::UInt16>(port));
        }
        catch (const std::exception& ex)
        {
            callback(false, ex.what());
            return;
        }

        auto self = shared_from_this();
        auto payload = std::make_shared<std::string>(std::move(data));
        // clang-format off
        loop.watchWrite(socket, [self, payload, destination, callback = std::move(callback)]() -> bool
        {
            try
            {
                const int wrote = self->socket.sendTo(payload->data(), static_cast<int>(payload->size()), *destination);
                if (wrote < 0)
                {
                    return false;
                }

                if (static_cast<std::size_t>(wrote) != payload->size())
                {
                    callback(false, "[PocoUdpSocket] The datagram could not be fully sent.");
                    return true;
                }

                callback(true, std::string());
                return true;
            }
            catch (const std::exception& ex)
            {
                callback(false, ex.what());
                return true;
            }
        });
        // clang-format on
    }

    void receiveFromAsync(int maxBytes, ReceiveFromCallback callback) override
    {
        if (closed)
        {
            callback(false, UdpDatagram{}, "[PocoUdpSocket] The socket is closed.");
            return;
        }

        const int capacity = maxBytes > kMaxDatagramBytes ? kMaxDatagramBytes : maxBytes;
        auto self = shared_from_this();
        // clang-format off
        loop.watchRead(socket, [self, capacity, callback = std::move(callback)]() -> bool
        {
            try
            {
                std::vector<char> buffer(static_cast<std::size_t>(capacity));
                Poco::Net::SocketAddress sender;
                const int received = self->socket.receiveFrom(buffer.data(), capacity, sender);
                if (received < 0)
                {
                    return false;
                }

                UdpDatagram datagram;
                if (received > 0)
                {
                    datagram.data.assign(buffer.data(), static_cast<std::size_t>(received));
                }

                datagram.host = sender.host().toString();
                datagram.port = sender.port();
                callback(true, datagram, "");
                return true;
            }
            catch (const std::exception& ex)
            {
                callback(false, UdpDatagram{}, ex.what());
                return true;
            }
        });
        // clang-format on
    }

    void close() override
    {
        closed = true;
        ManagedSocket::close(loop, socket);
    }

private:
    Poco::Net::DatagramSocket socket;
    EventLoop& loop;
    bool closed = false;
};

} // namespace

std::shared_ptr<UdpSocket> SocketTransport::bindUdp(varn::runtime::Runtime& runtime, const std::string& host, int port)
{
    Poco::Net::DatagramSocket socket(Poco::Net::SocketAddress(host, static_cast<Poco::UInt16>(port)), false);
    return std::make_shared<PocoUdpSocket>(std::move(socket), runtime.mainLoop());
}

} // namespace varn::socket
