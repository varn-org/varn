#pragma once

#include "varn/socket/SocketTransport.h"

#include <Poco/Net/SocketAddress.h>

#include <exception>
#include <string>

namespace varn::socket
{

class PocoSocketText
{
public:
    PocoSocketText() = delete;

    static constexpr const char* kSocketClosed = "The socket was closed.";
    static constexpr const char* kListenerClosed = "The listener was closed.";

    static SocketEndpoint endpoint(const Poco::Net::SocketAddress& address);
    static std::string describe(const SocketEndpoint& endpoint);
    static std::string target(const std::string& host, int port);
    static std::string failure(const std::string& subject, const std::exception& error);
    static std::string systemFailure(const std::string& subject, int code);
    static std::string bindFailure(const std::string& address, const std::exception& error);
    static std::string connectFailure(const std::string& address, const std::exception& error);
    static std::string connectFailure(const std::string& address, int code);
    static std::string resolveFailure(const std::string& host, const std::exception& error);

private:
    static std::string clause(std::string text);
};

} // namespace varn::socket
