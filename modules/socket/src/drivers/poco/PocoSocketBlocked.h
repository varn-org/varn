#pragma once

#include <Poco/Net/Socket.h>

#include <string>

namespace varn::socket
{

/// Tells why the platform blocked the packets of a socket, which it drops instead of refusing them, so a connect fails with the cause rather than at its deadline.
class PocoSocketBlocked
{
public:
    PocoSocketBlocked() = delete;

    static std::string reason(const Poco::Net::Socket& socket, const std::string& target);
};

} // namespace varn::socket
