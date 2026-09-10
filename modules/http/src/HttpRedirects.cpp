#include "HttpRedirects.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace varn::http::client
{
namespace
{

class RedirectHelpers
{
public:
    RedirectHelpers() = delete;

    static std::string lowered(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });

        return text;
    }

    /// Answers the scheme and authority of a url, which is what one origin is compared to another by.
    static std::string originOf(const std::string& url)
    {
        const std::size_t scheme = url.find("://");
        if (scheme == std::string::npos)
        {
            return {};
        }

        const std::size_t path = url.find('/', scheme + 3);
        return lowered(path == std::string::npos ? url : url.substr(0, path));
    }

    /// Answers a url with its query and fragment taken off, which a relative location replaces.
    static std::string withoutQuery(const std::string& url)
    {
        const std::size_t cut = url.find_first_of("?#");
        return cut == std::string::npos ? url : url.substr(0, cut);
    }

    static void erase(std::map<std::string, std::string>& headers, const std::string& name)
    {
        const std::string wanted = lowered(name);

        for (auto it = headers.begin(); it != headers.end();)
        {
            it = lowered(it->first) == wanted ? headers.erase(it) : std::next(it);
        }
    }

    /// Answers whether a redirect turns what was asked into a plain read of somewhere else.
    ///
    /// A 303 always does, and a 301 or a 302 does for anything but a GET or a HEAD, which is what every
    /// client has done since browsers settled it. A 307 and a 308 exist precisely to say it does not.
    static bool becomesGet(int status, const std::string& method)
    {
        if (status == 303)
        {
            return method != "GET" && method != "HEAD";
        }

        if (status == 301 || status == 302)
        {
            return method != "GET" && method != "HEAD";
        }

        return false;
    }
};

} // namespace

bool HttpRedirects::moved(int status)
{
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

std::string HttpRedirects::locationOf(const ClientResponse& response, const std::string& base)
{
    for (const auto& header : response.headers)
    {
        if (RedirectHelpers::lowered(header.first) == "location" && !header.second.empty())
        {
            return resolve(base, header.second);
        }
    }

    return {};
}

std::string HttpRedirects::resolve(const std::string& base, const std::string& location)
{
    if (location.find("://") != std::string::npos)
    {
        return location;
    }

    const std::string origin = RedirectHelpers::originOf(base);

    if (origin.empty())
    {
        return location;
    }

    // A location beginning with two slashes keeps the scheme and names a host of its own.
    if (location.rfind("//", 0) == 0)
    {
        const std::size_t scheme = base.find("://");
        return base.substr(0, scheme + 1) + location;
    }

    if (location.rfind('/', 0) == 0)
    {
        return origin + location;
    }

    // Anything else stands beside whatever the request already named.
    const std::string trimmed = RedirectHelpers::withoutQuery(base);
    const std::size_t slash = trimmed.find_last_of('/');

    if (slash == std::string::npos || slash < origin.size())
    {
        return origin + "/" + location;
    }

    return trimmed.substr(0, slash + 1) + location;
}

bool HttpRedirects::sameOrigin(const std::string& first, const std::string& second)
{
    return RedirectHelpers::originOf(first) == RedirectHelpers::originOf(second);
}

ClientResponse HttpRedirects::follow(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const ClientRequestOptions& options)
{
    std::string at = url;
    std::string verb = method;
    std::string payload = body;
    std::map<std::string, std::string> carried = headers;

    for (int hop = 0; hop <= options.maxRedirects; ++hop)
    {
        ClientResponse response = HttpClientPerform::perform(verb, at, carried, payload, options);

        if (options.redirects == RedirectPolicy::Manual || !moved(response.status))
        {
            return response;
        }

        const std::string next = locationOf(response, at);

        // A redirect with nowhere to go is the answer itself, which is what every client treats it as.
        if (next.empty())
        {
            return response;
        }

        if (options.redirects == RedirectPolicy::Error)
        {
            throw std::runtime_error("[HttpClientModule] The request was redirected and redirects were refused.");
        }

        if (hop == options.maxRedirects)
        {
            break;
        }

        // Credentials belong to the host they were meant for, so they do not travel to another one.
        if (!sameOrigin(at, next))
        {
            RedirectHelpers::erase(carried, "authorization");
            RedirectHelpers::erase(carried, "cookie");
            RedirectHelpers::erase(carried, "proxy-authorization");
        }

        if (RedirectHelpers::becomesGet(response.status, verb))
        {
            verb = "GET";
            payload.clear();
            RedirectHelpers::erase(carried, "content-length");
            RedirectHelpers::erase(carried, "content-type");
        }

        at = next;
    }

    throw std::runtime_error("[HttpClientModule] The request was redirected more times than it was allowed to be.");
}

void HttpRedirects::followStream(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const ClientRequestOptions& options,
    const StreamResponseFn& onResponse,
    const StreamChunkFn& onChunk)
{
    std::string at = url;
    std::string verb = method;
    std::string payload = body;
    std::map<std::string, std::string> carried = headers;

    for (int hop = 0; hop <= options.maxRedirects; ++hop)
    {
        ClientResponse pointer;
        bool moving = false;

        // A redirect's own body is nothing a caller asked for, so it is swallowed rather than streamed.
        // clang-format off
        auto answered = [&](int status, const ResponseHeaders& responseHeaders)
        {
            moving = options.redirects != RedirectPolicy::Manual && moved(status);

            if (!moving)
            {
                onResponse(status, responseHeaders);
                return;
            }

            pointer.status = status;
            pointer.headers = responseHeaders;
        };

        auto received = [&](const char* chunk, std::size_t length)
        {
            if (!moving)
            {
                onChunk(chunk, length);
            }
        };
        // clang-format on

        HttpClientPerform::performStream(verb, at, carried, payload, options, answered, received);

        if (!moving)
        {
            return;
        }

        const std::string next = locationOf(pointer, at);

        if (next.empty())
        {
            onResponse(pointer.status, pointer.headers);
            return;
        }

        if (options.redirects == RedirectPolicy::Error)
        {
            throw std::runtime_error("[HttpClientModule] The request was redirected and redirects were refused.");
        }

        if (hop == options.maxRedirects)
        {
            break;
        }

        if (!sameOrigin(at, next))
        {
            RedirectHelpers::erase(carried, "authorization");
            RedirectHelpers::erase(carried, "cookie");
            RedirectHelpers::erase(carried, "proxy-authorization");
        }

        if (RedirectHelpers::becomesGet(pointer.status, verb))
        {
            verb = "GET";
            payload.clear();
            RedirectHelpers::erase(carried, "content-length");
            RedirectHelpers::erase(carried, "content-type");
        }

        at = next;
    }

    throw std::runtime_error("[HttpClientModule] The request was redirected more times than it was allowed to be.");
}

} // namespace varn::http::client
