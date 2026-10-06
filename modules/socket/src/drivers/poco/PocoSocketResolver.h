#pragma once

#include <Poco/Net/SocketAddress.h>

#include <functional>
#include <optional>
#include <string>

namespace varn::runtime
{
class Runtime;
}

namespace varn::socket
{

class PocoSocketResolver
{
public:
    using AddressCallback = std::function<void(std::optional<Poco::Net::SocketAddress> address, const std::string& error)>;

    PocoSocketResolver() = delete;

    static bool numeric(const std::string& host);
    static void resolveAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, AddressCallback callback);
    static std::optional<Poco::Net::SocketAddress> resolve(const std::string& host, int port, std::string& error);
};

} // namespace varn::socket
