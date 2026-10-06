#pragma once

#include <Poco/Net/Socket.h>

#include <string>

namespace varn::socket
{

class PocoSocketBlocked
{
public:
    PocoSocketBlocked() = delete;

    static std::string reason(const Poco::Net::Socket& socket, const std::string& target);
};

} // namespace varn::socket
