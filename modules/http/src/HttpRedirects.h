#pragma once

#include "HttpClientPerform.h"

#include <string>

namespace varn::http::client
{

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
