#pragma once

#include "varn/http/HttpClientPerform.h"

#include <map>
#include <string>

namespace varn::http::client
{

/// Follows the redirects a response asks for, above the driver so every target answers the same way.
///
/// A platform transport follows a redirect on its own and a driver blocks it, precisely so the four
/// targets agree. Following it here rather than in each driver keeps that agreement while giving the
/// caller the choice every other client offers.
class HttpRedirects
{
public:
    HttpRedirects() = delete;

    static ClientResponse follow(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const ClientRequestOptions& options);

    static void followStream(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const ClientRequestOptions& options,
        const StreamResponseFn& onResponse,
        const StreamChunkFn& onChunk);

    /// Answers whether a status is one that asks the caller to go somewhere else.
    static bool moved(int status);

    /// Answers where a response points, resolved against the request it answered.
    static std::string locationOf(const ClientResponse& response, const std::string& base);

    /// Answers an absolute url for a location that may be absolute, host relative or path relative.
    static std::string resolve(const std::string& base, const std::string& location);

    /// Answers whether two urls address the same origin, which decides what credentials may travel on.
    static bool sameOrigin(const std::string& first, const std::string& second);
};

} // namespace varn::http::client
