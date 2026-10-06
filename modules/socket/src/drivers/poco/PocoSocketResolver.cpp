#include "PocoSocketResolver.h"

#include "PocoSocketText.h"

#include "varn/socket/SocketTransport.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"

#include <Poco/Net/DNS.h>
#include <Poco/Net/HostEntry.h>
#include <Poco/Net/IPAddress.h>

#include <exception>
#include <utility>

namespace varn::socket
{

bool PocoSocketResolver::numeric(const std::string& host)
{
    Poco::Net::IPAddress address;
    return Poco::Net::IPAddress::tryParse(host, address);
}

// Runs a lookup at once for a numeric address and on the I/O pool for a name before calling back on the loop, so a lookup never blocks the loop thread.
template <typename LookUp, typename Callback>
void PocoSocketResolver::lookUpAsync(varn::runtime::Runtime& runtime, const std::string& host, LookUp lookUp, Callback callback)
{
    if (numeric(host))
    {
        std::string error;
        auto found = lookUp(error);
        callback(std::move(found), error);
        return;
    }

    varn::runtime::EventLoop* loop = &runtime.scheduler().mainLoop();
    // clang-format off
    runtime.scheduler().ioPool().post([loop, lookUp = std::move(lookUp), callback = std::move(callback)]() mutable
    {
        std::string error;
        auto found = lookUp(error);
        loop->post([found = std::move(found), error = std::move(error), callback = std::move(callback)]() mutable
        {
            callback(std::move(found), error);
        });
    });
    // clang-format on
}

// Answers the one address a socket uses for the host.
void PocoSocketResolver::resolveAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, AddressCallback callback)
{
    // clang-format off
    lookUpAsync(runtime, host, [host, port](std::string& error) { return resolve(host, port, error); }, std::move(callback));
    // clang-format on
}

// Answers the address a socket uses for the host, which blocks on the resolver of the system for a name, and writes the reason to `error` when there is none.
std::optional<Poco::Net::SocketAddress> PocoSocketResolver::resolve(const std::string& host, int port, std::string& error)
{
    try
    {
        return Poco::Net::SocketAddress(host, static_cast<Poco::UInt16>(port));
    }
    catch (const std::exception& ex)
    {
        error = PocoSocketText::resolveFailure(host, ex);
        return std::nullopt;
    }
}

// Answers every address of the host once, in the order of the resolver of the system, and a numeric address as it is.
std::vector<std::string> PocoSocketResolver::addresses(const std::string& host, std::string& error)
{
    Poco::Net::IPAddress numericAddress;
    if (Poco::Net::IPAddress::tryParse(host, numericAddress))
    {
        return {numericAddress.toString()};
    }

    std::vector<std::string> found;
    try
    {
        const Poco::Net::HostEntry entry = Poco::Net::DNS::hostByName(host);
        for (const auto& address : entry.addresses())
        {
            found.push_back(address.toString());
        }
    }
    catch (const std::exception& ex)
    {
        error = PocoSocketText::resolveFailure(host, ex);
    }

    return found;
}

void PocoSocketResolver::addressesAsync(varn::runtime::Runtime& runtime, const std::string& host, AddressesCallback callback)
{
    // clang-format off
    lookUpAsync(runtime, host, [host](std::string& error) { return addresses(host, error); }, std::move(callback));
    // clang-format on
}

void SocketTransport::resolveAsync(varn::runtime::Runtime& runtime, const std::string& host, ResolveCallback callback)
{
    // clang-format off
    PocoSocketResolver::addressesAsync(runtime, host, [callback = std::move(callback)](std::vector<std::string> found, const std::string& error)
    {
        callback(error.empty(), std::move(found), error);
    });
    // clang-format on
}

} // namespace varn::socket
