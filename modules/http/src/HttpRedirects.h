#pragma once

#include "HttpClientPerform.h"

#include <string>

namespace varn::http::client
{

/// Runs a transfer on the thread of a pool and follows the redirects its responses ask for, above the driver so every target answers the same way.
///
/// A platform transport follows a redirect on its own and a driver blocks it, precisely so the four targets agree.
/// Following it here rather than in each driver keeps that agreement while giving the caller the choice every other client offers.
class HttpRedirects
{
public:
    HttpRedirects() = delete;

    static void run(HttpClientConnections& connections, HttpClientTransfer& transfer);

private:
    static void follow(HttpClientConnections& connections, HttpClientTransfer& transfer);
    static bool moved(int status);
    static std::string locationOf(const Headers& headers, const std::string& base);
    static std::string resolve(const std::string& base, const std::string& location);
    static bool sameOrigin(const std::string& first, const std::string& second);
};

} // namespace varn::http::client
