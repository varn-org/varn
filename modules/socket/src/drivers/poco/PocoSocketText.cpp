#include "PocoSocketText.h"

#include <Poco/Error.h>
#include <Poco/Exception.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketDefs.h>

#include <string>

namespace varn::socket
{

// Answers the host and port of an internet address, and the path with no port for a Unix-domain address.
SocketEndpoint PocoSocketText::endpoint(const Poco::Net::SocketAddress& address)
{
#if defined(POCO_HAS_UNIX_SOCKET)
    if (address.family() == Poco::Net::SocketAddress::UNIX_LOCAL)
    {
        return SocketEndpoint{address.toString(), std::nullopt};
    }
#endif

    return SocketEndpoint{address.host().toString(), address.port()};
}

// Writes an endpoint the way an error names it, with an IPv6 host in brackets.
std::string PocoSocketText::describe(const SocketEndpoint& endpoint)
{
    if (!endpoint.port)
    {
        return endpoint.host;
    }

    return target(endpoint.host, *endpoint.port);
}

std::string PocoSocketText::target(const std::string& host, int port)
{
    if (host.find(':') != std::string::npos)
    {
        return "[" + host + "]:" + std::to_string(port);
    }

    return host + ":" + std::to_string(port);
}

// Completes a subject such as "The receive from 127.0.0.1:5000 failed" with the text of the error and the system error it carries.
std::string PocoSocketText::failure(const std::string& subject, const std::exception& error)
{
    const auto* poco = dynamic_cast<const Poco::Exception*>(&error);
    if (poco == nullptr)
    {
        return subject + ": " + clause(error.what()) + ".";
    }

    if (poco->code() == 0)
    {
        return subject + ": " + clause(poco->displayText()) + ".";
    }

    return subject + ": " + clause(poco->displayText()) + " (system error " + std::to_string(poco->code()) + ").";
}

std::string PocoSocketText::systemFailure(const std::string& subject, int code)
{
#if defined(_WIN32)
    const std::string message = Poco::Error::getMessage(static_cast<unsigned long>(code));
#else
    const std::string message = Poco::Error::getMessage(code);
#endif

    return subject + ": " + clause(message) + " (system error " + std::to_string(code) + ").";
}

std::string PocoSocketText::bindFailure(const std::string& address, const std::exception& error)
{
    const auto* poco = dynamic_cast<const Poco::Exception*>(&error);
    if (poco != nullptr && poco->code() == POCO_EADDRINUSE)
    {
        return "The address " + address + " is already in use.";
    }

    return failure("The address " + address + " could not be bound", error);
}

std::string PocoSocketText::connectFailure(const std::string& address, const std::exception& error)
{
    if (dynamic_cast<const Poco::TimeoutException*>(&error) != nullptr)
    {
        return "The connection to " + address + " timed out.";
    }

    const auto* poco = dynamic_cast<const Poco::Exception*>(&error);
    if (poco != nullptr && poco->code() != 0)
    {
        return connectFailure(address, poco->code());
    }

    return failure("The connection to " + address + " failed", error);
}

std::string PocoSocketText::connectFailure(const std::string& address, int code)
{
    if (code == POCO_ECONNREFUSED)
    {
        return "The connection to " + address + " was refused.";
    }

    if (code == POCO_ETIMEDOUT)
    {
        return "The connection to " + address + " timed out.";
    }

    return systemFailure("The connection to " + address + " failed", code);
}

// A name that does not exist is reported on its own, since the resolver adds nothing to it.
std::string PocoSocketText::resolveFailure(const std::string& host, const std::exception& error)
{
    if (dynamic_cast<const Poco::Net::HostNotFoundException*>(&error) != nullptr || dynamic_cast<const Poco::Net::NoAddressFoundException*>(&error) != nullptr)
    {
        return "The host \"" + host + "\" could not be resolved.";
    }

    const auto* poco = dynamic_cast<const Poco::Exception*>(&error);
    return "The host \"" + host + "\" could not be resolved: " + clause(poco != nullptr ? poco->displayText() : std::string(error.what())) + ".";
}

// Drops the line breaks and the final period a system message ends with, so the text continues a sentence.
std::string PocoSocketText::clause(std::string text)
{
    while (!text.empty() && (text.back() == '.' || text.back() == ' ' || text.back() == '\r' || text.back() == '\n'))
    {
        text.pop_back();
    }

    return text;
}

} // namespace varn::socket
