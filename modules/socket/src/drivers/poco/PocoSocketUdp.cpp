#include "varn/socket/SocketTransport.h"

#include "PocoSocketResolver.h"
#include "PocoSocketStream.h"
#include "PocoSocketText.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"

#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/IPAddress.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/SocketDefs.h>

#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace varn::socket
{

using varn::runtime::EventLoop;

namespace
{

constexpr int kMaxDatagramBytes = 65536;

using AddressedDatagrams = std::vector<std::pair<Poco::Net::SocketAddress, std::string>>;

class PocoUdpSocket : public UdpSocket, public std::enable_shared_from_this<PocoUdpSocket>
{
public:
    PocoUdpSocket(Poco::Net::DatagramSocket socket, varn::runtime::Runtime& runtime)
        : socket(std::move(socket))
        , runtime(runtime)
        , loop(runtime.mainLoop())
        , bound(PocoSocketText::endpoint(this->socket.address()))
        , boundText(PocoSocketText::describe(bound))
    {
        this->socket.setBlocking(false);
    }

    // A datagram addressed by a name leaves once the name resolved on the I/O pool, so it may leave after a later one addressed by a number.
    void sendToAsync(std::string host, int port, std::string data, SendCallback callback) override
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        auto self = shared_from_this();
        auto payload = std::make_shared<std::string>(std::move(data));
        // clang-format off
        PocoSocketResolver::resolveAsync(runtime, host, port, [self, payload, callback = std::move(callback)](std::optional<Poco::Net::SocketAddress> address, const std::string& error)
        {
            if (!address)
            {
                callback(false, error);
                return;
            }

            if (self->closed)
            {
                callback(false, PocoSocketText::kSocketClosed);
                return;
            }

            self->watchSend(std::make_shared<Poco::Net::SocketAddress>(*address), payload, callback);
        });
        // clang-format on
    }

    void receiveFromAsync(int maxBytes, ReceiveFromCallback callback) override
    {
        if (closed)
        {
            callback(false, UdpDatagram{}, PocoSocketText::kSocketClosed);
            return;
        }

        const int capacity = maxBytes > kMaxDatagramBytes ? kMaxDatagramBytes : maxBytes;
        auto self = shared_from_this();
        // clang-format off
        loop.watchRead(socket, [self, capacity, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(false, UdpDatagram{}, PocoSocketText::kSocketClosed);
                return true;
            }

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
                callback(false, UdpDatagram{}, PocoSocketText::failure("The receive on " + self->boundText + " failed", ex));
                return true;
            }
        });
        // clang-format on
    }

    // Sends every datagram in order once the socket takes them, settling once all left or at the first that failed, and resolves the names of the batch on the I/O pool first.
    void sendManyAsync(std::vector<UdpDatagram> datagrams, SendCallback callback) override
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        if (datagrams.empty())
        {
            callback(true, std::string());
            return;
        }

        auto self = shared_from_this();
        bool named = false;
        for (const auto& datagram : datagrams)
        {
            named = named || !PocoSocketResolver::numeric(datagram.host);
        }

        if (!named)
        {
            std::string error;
            auto batch = address(std::move(datagrams), error);
            sendBatch(std::move(batch), error, std::move(callback));
            return;
        }

        // clang-format off
        runtime.ioPool().post([self, datagrams = std::move(datagrams), callback = std::move(callback)]() mutable
        {
            std::string error;
            auto batch = address(std::move(datagrams), error);
            self->loop.post([self, batch = std::move(batch), error = std::move(error), callback = std::move(callback)]() mutable
            {
                self->sendBatch(std::move(batch), error, std::move(callback));
            });
        });
        // clang-format on
    }

    // Waits until a datagram arrives and then takes every datagram already waiting, up to the count asked for.
    void receiveManyAsync(int maxDatagrams, int maxBytes, ReceiveManyCallback callback) override
    {
        if (closed)
        {
            callback(false, {}, PocoSocketText::kSocketClosed);
            return;
        }

        const int capacity = maxBytes > kMaxDatagramBytes ? kMaxDatagramBytes : maxBytes;
        auto self = shared_from_this();
        // clang-format off
        loop.watchRead(socket, [self, maxDatagrams, capacity, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(false, {}, PocoSocketText::kSocketClosed);
                return true;
            }

            std::vector<char> buffer(static_cast<std::size_t>(capacity));
            std::vector<UdpDatagram> datagrams;
            try
            {
                while (datagrams.size() < static_cast<std::size_t>(maxDatagrams))
                {
                    Poco::Net::SocketAddress sender;
                    const int received = self->socket.receiveFrom(buffer.data(), capacity, sender);
                    if (received < 0)
                    {
                        break;
                    }

                    UdpDatagram& datagram = datagrams.emplace_back();
                    datagram.data.assign(buffer.data(), static_cast<std::size_t>(received));
                    datagram.host = sender.host().toString();
                    datagram.port = sender.port();
                }
            }
            catch (const std::exception& ex)
            {
                // The datagrams already taken are delivered, and a lasting failure reaches the next receive.
                if (datagrams.empty())
                {
                    callback(false, {}, PocoSocketText::failure("The receive on " + self->boundText + " failed", ex));
                    return true;
                }
            }

            if (datagrams.empty())
            {
                return false;
            }

            callback(true, std::move(datagrams), "");
            return true;
        });
        // clang-format on
    }

    // Sets the only peer the socket sends to with `send` and receives from, once its name resolved.
    void connectAsync(std::string host, int port, SendCallback callback) override
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        auto self = shared_from_this();
        // clang-format off
        PocoSocketResolver::resolveAsync(runtime, host, port, [self, callback = std::move(callback)](std::optional<Poco::Net::SocketAddress> address, const std::string& error)
        {
            if (!address)
            {
                callback(false, error);
                return;
            }

            if (self->closed)
            {
                callback(false, PocoSocketText::kSocketClosed);
                return;
            }

            try
            {
                self->socket.connect(*address);
            }
            catch (const std::exception& ex)
            {
                callback(false, PocoSocketText::failure("The socket on " + self->boundText + " could not be connected to " + address->toString(), ex));
                return;
            }

            self->peer = std::make_shared<Poco::Net::SocketAddress>(*address);
            callback(true, std::string());
        });
        // clang-format on
    }

    void sendAsync(std::string data, SendCallback callback) override
    {
        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        if (!peer)
        {
            callback(false, "The socket is not connected. Connect it with \"connect\" before calling \"send\".");
            return;
        }

        watchSend(nullptr, std::make_shared<std::string>(std::move(data)), std::move(callback));
    }

    void setBroadcast(bool enabled) override
    {
        if (closed)
        {
            throw std::runtime_error(PocoSocketText::kSocketClosed);
        }

        try
        {
            socket.setBroadcast(enabled);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error(PocoSocketText::failure("The option \"SO_BROADCAST\" could not be set on " + boundText, ex));
        }
    }

    void joinGroup(const std::string& group) override { changeGroup(group, true); }

    void leaveGroup(const std::string& group) override { changeGroup(group, false); }

    const SocketEndpoint& endpoint() const override { return bound; }

    void close() override
    {
        closed = true;
        ManagedSocket::close(loop, socket);
    }

private:
    // Answers every datagram with its address, resolving a name where the caller allows a block, and stops at the first that has none.
    static AddressedDatagrams address(std::vector<UdpDatagram> datagrams, std::string& error)
    {
        AddressedDatagrams batch;
        batch.reserve(datagrams.size());
        for (std::size_t index = 0; index < datagrams.size(); ++index)
        {
            std::string reason;
            auto destination = PocoSocketResolver::resolve(datagrams[index].host, datagrams[index].port, reason);
            if (!destination)
            {
                error = "The datagram at index " + std::to_string(index + 1) + " was not sent. " + reason;
                return {};
            }

            batch.emplace_back(*destination, std::move(datagrams[index].data));
        }

        return batch;
    }

    void sendBatch(AddressedDatagrams datagrams, const std::string& error, SendCallback callback)
    {
        if (!error.empty())
        {
            callback(false, error);
            return;
        }

        if (closed)
        {
            callback(false, PocoSocketText::kSocketClosed);
            return;
        }

        auto self = shared_from_this();
        auto batch = std::make_shared<AddressedDatagrams>(std::move(datagrams));
        auto next = std::make_shared<std::size_t>(0);
        // clang-format off
        loop.watchWrite(socket, [self, batch, next, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(false, PocoSocketText::kSocketClosed);
                return true;
            }

            try
            {
                while (*next < batch->size())
                {
                    const auto& [destination, payload] = (*batch)[*next];
                    const int wrote = self->socket.sendTo(payload.data(), static_cast<int>(payload.size()), destination);
                    if (wrote < 0)
                    {
                        return false;
                    }

                    if (static_cast<std::size_t>(wrote) != payload.size())
                    {
                        callback(false, "The datagram at index " + std::to_string(*next + 1) + " could not be fully sent.");
                        return true;
                    }

                    ++*next;
                }

                callback(true, std::string());
                return true;
            }
            catch (const std::exception& ex)
            {
                const auto& destination = (*batch)[*next].first;
                callback(false, PocoSocketText::failure("The datagram at index " + std::to_string(*next + 1) + " to " + destination.toString() + " could not be sent", ex));
                return true;
            }
        });
        // clang-format on
    }

    // Sends one datagram to the destination, or to the connected peer when there is none.
    void watchSend(std::shared_ptr<Poco::Net::SocketAddress> destination, std::shared_ptr<std::string> payload, SendCallback callback)
    {
        auto self = shared_from_this();
        // clang-format off
        loop.watchWrite(socket, [self, destination, payload, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(false, PocoSocketText::kSocketClosed);
                return true;
            }

            const std::string target = destination ? destination->toString() : self->peer->toString();
            try
            {
                const int size = static_cast<int>(payload->size());
                const int wrote = destination ? self->socket.sendTo(payload->data(), size, *destination) : self->socket.sendBytes(payload->data(), size);
                if (wrote < 0)
                {
                    return false;
                }

                if (static_cast<std::size_t>(wrote) != payload->size())
                {
                    callback(false, "The datagram to " + target + " could not be fully sent.");
                    return true;
                }

                callback(true, std::string());
                return true;
            }
            catch (const std::exception& ex)
            {
                callback(false, PocoSocketText::failure("The datagram to " + target + " could not be sent", ex));
                return true;
            }
        });
        // clang-format on
    }

    // Joins or leaves a multicast group on the interface the system picks for it.
    void changeGroup(const std::string& group, bool join)
    {
        if (closed)
        {
            throw std::runtime_error(PocoSocketText::kSocketClosed);
        }

        Poco::Net::IPAddress address;
        if (!Poco::Net::IPAddress::tryParse(group, address) || !address.isMulticast())
        {
            throw std::runtime_error("The address \"" + group + "\" is not a multicast group.");
        }

        const std::string action = join ? "joined" : "left";
        try
        {
            if (address.family() == Poco::Net::IPAddress::IPv4)
            {
                ip_mreq request{};
                std::memcpy(&request.imr_multiaddr, address.addr(), sizeof(request.imr_multiaddr));
                request.imr_interface.s_addr = htonl(INADDR_ANY);
                socket.impl()->setRawOption(IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &request, sizeof(request));
                return;
            }

            ipv6_mreq request{};
            std::memcpy(&request.ipv6mr_multiaddr, address.addr(), sizeof(request.ipv6mr_multiaddr));
            request.ipv6mr_interface = 0;
            socket.impl()->setRawOption(IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, &request, sizeof(request));
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error(PocoSocketText::failure("The group \"" + group + "\" could not be " + action + " on " + boundText, ex));
        }
    }

    Poco::Net::DatagramSocket socket;
    varn::runtime::Runtime& runtime;
    EventLoop& loop;
    SocketEndpoint bound;
    std::string boundText;
    std::shared_ptr<Poco::Net::SocketAddress> peer;
    bool closed = false;
};

} // namespace

// Binds without `SO_REUSEADDR` or `SO_REUSEPORT`, since either lets a second UDP socket share a busy port on Linux, and with `SO_EXCLUSIVEADDRUSE` on Windows, which Poco sets when reuse is off.
void SocketTransport::bindUdpAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, BindCallback callback)
{
    varn::runtime::Runtime* rt = &runtime;
    // clang-format off
    PocoSocketResolver::resolveAsync(runtime, host, port, [rt, callback = std::move(callback)](std::optional<Poco::Net::SocketAddress> address, const std::string& error)
    {
        if (!address)
        {
            callback(nullptr, error);
            return;
        }

        std::shared_ptr<UdpSocket> socket;
        try
        {
            socket = std::make_shared<PocoUdpSocket>(Poco::Net::DatagramSocket(*address, false, false), *rt);
        }
        catch (const std::exception& ex)
        {
            callback(nullptr, PocoSocketText::bindFailure(address->toString(), ex));
            return;
        }

        callback(std::move(socket), "");
    });
    // clang-format on
}

} // namespace varn::socket
