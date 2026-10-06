#include "PocoSocketResolver.h"

#include "PocoSocketText.h"

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"

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

// Calls back at once for a numeric address and looks a name up on the I/O pool before calling back on the loop, so a lookup never blocks the loop thread.
void PocoSocketResolver::resolveAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, AddressCallback callback)
{
    if (numeric(host))
    {
        std::string error;
        auto address = resolve(host, port, error);
        callback(std::move(address), error);
        return;
    }

    varn::runtime::EventLoop* loop = &runtime.mainLoop();
    // clang-format off
    runtime.ioPool().post([loop, host, port, callback = std::move(callback)]() mutable
    {
        std::string error;
        auto address = resolve(host, port, error);
        loop->post([address = std::move(address), error = std::move(error), callback = std::move(callback)]() mutable
        {
            callback(std::move(address), error);
        });
    });
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

} // namespace varn::socket
