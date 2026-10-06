#pragma once

#include <Poco/Net/SocketAddress.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

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
    using AddressesCallback = std::function<void(std::vector<std::string> addresses, const std::string& error)>;

    PocoSocketResolver() = delete;

    static bool numeric(const std::string& host);
    static void resolveAsync(varn::runtime::Runtime& runtime, const std::string& host, int port, AddressCallback callback);
    static std::optional<Poco::Net::SocketAddress> resolve(const std::string& host, int port, std::string& error);
    static void addressesAsync(varn::runtime::Runtime& runtime, const std::string& host, AddressesCallback callback);

private:
    static std::vector<std::string> addresses(const std::string& host, std::string& error);
    template <typename LookUp, typename Callback>
    static void lookUpAsync(varn::runtime::Runtime& runtime, const std::string& host, LookUp lookUp, Callback callback);
};

} // namespace varn::socket
