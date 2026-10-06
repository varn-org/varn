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

#include <algorithm>
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

// A receive takes every datagram into a buffer that holds the largest one UDP carries, so a datagram is never cut short by the system and one larger than the receive takes is reported the same way everywhere.
constexpr int kMaxDatagramBytes = 65536;

using AddressedDatagrams = std::vector<std::pair<Poco::Net::SocketAddress, std::string>>;

class PocoUdpSocket : public UdpSocket, public std::enable_shared_from_this<PocoUdpSocket>
{
public:
    PocoUdpSocket(Poco::Net::DatagramSocket socket, varn::runtime::Runtime& runtime, std::size_t maxQueuedBytes)
        : socket(std::move(socket))
        , runtime(runtime)
        , loop(runtime.scheduler().mainLoop())
        , maxQueuedBytes(maxQueuedBytes)
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

        if (!charge(data.size(), callback))
        {
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

        if (!deferred.empty())
        {
            callback(false, UdpDatagram{}, std::exchange(deferred, std::string()));
            return;
        }

        auto self = shared_from_this();
        // clang-format off
        loop.watchRead(socket, [self, maxBytes, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(false, UdpDatagram{}, PocoSocketText::kSocketClosed);
                return true;
            }

            UdpDatagram datagram;
            std::string error;
            if (!self->receiveOne(maxBytes, datagram, error))
            {
                return false;
            }

            callback(error.empty(), datagram, error);
            return true;
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

        std::size_t bytes = 0;
        for (const auto& datagram : datagrams)
        {
            bytes += datagram.data.size();
        }

        if (!charge(bytes, callback))
        {
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
        runtime.scheduler().ioPool().post([self, datagrams = std::move(datagrams), callback = std::move(callback)]() mutable
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
    // The datagrams taken before a failure are delivered, and the failure is answered by the next receive.
    void receiveManyAsync(int maxDatagrams, int maxBytes, ReceiveManyCallback callback) override
    {
        if (closed)
        {
            callback(false, {}, PocoSocketText::kSocketClosed);
            return;
        }

        if (!deferred.empty())
        {
            callback(false, {}, std::exchange(deferred, std::string()));
            return;
        }

        auto self = shared_from_this();
        // clang-format off
        loop.watchRead(socket, [self, maxDatagrams, maxBytes, callback = std::move(callback)]() -> bool
        {
            if (self->closed)
            {
                callback(false, {}, PocoSocketText::kSocketClosed);
                return true;
            }

            std::vector<UdpDatagram> datagrams;
            std::string error;
            while (datagrams.size() < static_cast<std::size_t>(maxDatagrams) && error.empty())
            {
                UdpDatagram datagram;
                if (!self->receiveOne(maxBytes, datagram, error))
                {
                    break;
                }

                if (error.empty())
                {
                    datagrams.push_back(std::move(datagram));
                }
            }

            if (datagrams.empty() && error.empty())
            {
                return false;
            }

            if (datagrams.empty())
            {
                callback(false, {}, error);
                return true;
            }

            self->deferred = std::move(error);
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

        if (!charge(data.size(), callback))
        {
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
    // Counts the bytes of a send until it settles and refuses one that would pass `maxQueuedBytes`, so sends that nobody awaits cannot grow without a bound.
    bool charge(std::size_t bytes, SendCallback& callback)
    {
        if (bytes > maxQueuedBytes - std::min(queuedBytes, maxQueuedBytes))
        {
            callback(false, "The socket already queues " + std::to_string(queuedBytes) + " bytes, so a send of " + std::to_string(bytes) + " bytes would pass its limit of " + std::to_string(maxQueuedBytes) + " bytes.");
            return false;
        }

        queuedBytes += bytes;
        auto self = shared_from_this();
        // clang-format off
        callback = [self, bytes, settle = std::move(callback)](bool ok, const std::string& error)
        {
            self->queuedBytes -= std::min(bytes, self->queuedBytes);
            settle(ok, error);
        };
        // clang-format on
        return true;
    }

    // Takes one waiting datagram into the buffer of the socket and answers whether one was waiting, refusing a datagram larger than the receive takes with a reason in `error`.
    bool receiveOne(int maxBytes, UdpDatagram& datagram, std::string& error)
    {
        if (!receiveBuffer)
        {
            receiveBuffer = std::make_unique_for_overwrite<char[]>(kMaxDatagramBytes);
        }

        Poco::Net::SocketAddress sender;
        int received = 0;
        try
        {
            received = socket.receiveFrom(receiveBuffer.get(), kMaxDatagramBytes, sender);
        }
        catch (const std::exception& ex)
        {
            error = PocoSocketText::failure("The receive on " + boundText + " failed", ex);
            return true;
        }

        if (received < 0)
        {
            return false;
        }

        if (received > maxBytes)
        {
            error = "The datagram of " + std::to_string(received) + " bytes from " + sender.toString() + " is larger than the " + std::to_string(maxBytes) + " bytes the receive on " + boundText + " takes.";
            return true;
        }

        datagram.data.assign(receiveBuffer.get(), static_cast<std::size_t>(received));
        datagram.host = sender.host().toString();
        datagram.port = sender.port();
        return true;
    }

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

            std::string error;
            try
            {
                while (*next < batch->size() && error.empty())
                {
                    const auto& [destination, payload] = (*batch)[*next];
                    const int wrote = self->socket.sendTo(payload.data(), static_cast<int>(payload.size()), destination);
                    if (wrote < 0)
                    {
                        return false;
                    }

                    if (static_cast<std::size_t>(wrote) != payload.size())
                    {
                        error = "The datagram at index " + std::to_string(*next + 1) + " could not be fully sent.";
                        continue;
                    }

                    ++*next;
                }
            }
            catch (const std::exception& ex)
            {
                const auto& destination = (*batch)[*next].first;
                error = PocoSocketText::failure("The datagram at index " + std::to_string(*next + 1) + " to " + destination.toString() + " could not be sent", ex);
            }

            callback(error.empty(), error);
            return true;
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
            std::string error;
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
                    error = "The datagram to " + target + " could not be fully sent.";
                }
            }
            catch (const std::exception& ex)
            {
                error = PocoSocketText::failure("The datagram to " + target + " could not be sent", ex);
            }

            callback(error.empty(), error);
            return true;
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
    std::size_t maxQueuedBytes;
    std::size_t queuedBytes = 0;
    SocketEndpoint bound;
    std::string boundText;
    std::shared_ptr<Poco::Net::SocketAddress> peer;
    std::unique_ptr<char[]> receiveBuffer;
    std::string deferred;
    bool closed = false;
};

} // namespace

// Binds without `SO_REUSEADDR` or `SO_REUSEPORT`, since either lets a second UDP socket share a busy port on Linux, and with `SO_EXCLUSIVEADDRUSE` on Windows, which Poco sets when reuse is off.
void SocketTransport::bindUdpAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, std::size_t maxQueuedBytes, BindCallback callback)
{
    varn::runtime::Runtime* rt = &runtime;
    // clang-format off
    PocoSocketResolver::resolveAsync(runtime, host, port, [rt, maxQueuedBytes, callback = std::move(callback)](std::optional<Poco::Net::SocketAddress> address, const std::string& error)
    {
        if (!address)
        {
            callback(nullptr, error);
            return;
        }

        std::shared_ptr<UdpSocket> socket;
        try
        {
            socket = std::make_shared<PocoUdpSocket>(Poco::Net::DatagramSocket(*address, false, false), *rt, maxQueuedBytes);
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
