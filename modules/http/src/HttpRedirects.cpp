#include "HttpRedirects.h"

#include "HttpClientFailure.h"
#include "HttpClientTransfer.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <optional>
#include <utility>

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

    /// Answers the scheme and authority of a URL, which is what one origin is compared to another by.
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

    /// Answers a URL with its query and fragment taken off, which a relative location replaces.
    static std::string withoutQuery(const std::string& url)
    {
        const std::size_t cut = url.find_first_of("?#");
        return cut == std::string::npos ? url : url.substr(0, cut);
    }

    static void erase(Headers& headers, const std::string& name)
    {
        const std::string wanted = lowered(name);

        // clang-format off
        headers.erase(std::remove_if(headers.begin(), headers.end(), [&](const auto& header) { return lowered(header.first) == wanted; }), headers.end());
        // clang-format on
    }

    /// Answers whether a redirect turns what was asked into a plain read of somewhere else.
    ///
    /// A 303 always does, and a 301 or a 302 does for anything but a GET or a HEAD, which is what every client has done since browsers settled it.
    /// A 307 and a 308 exist precisely to say it does not.
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

std::string HttpRedirects::locationOf(const Headers& headers, const std::string& base)
{
    for (const auto& header : headers)
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

// Runs every hop of a transfer and ends it with what the last one answered, so no failure of a transport escapes the job of the pool.
void HttpRedirects::run(HttpClientConnections& connections, HttpClientTransfer& transfer)
{
    try
    {
        follow(connections, transfer);
        transfer.succeed();
    }
    catch (const HttpClientFailure& failure)
    {
        transfer.fail(failure.error());
    }
    catch (const std::exception& failure)
    {
        transfer.fail(Error{ErrorCode::Network, failure.what()});
    }
    catch (...)
    {
        transfer.fail(Error{ErrorCode::Network, "[HttpClient] The request failed with a non-standard error."});
    }
}

void HttpRedirects::follow(HttpClientConnections& connections, HttpClientTransfer& transfer)
{
    const Request& request = transfer.request();
    Hop hop{request.method, request.url, request.headers, true};

    for (int count = 0;; ++count)
    {
        std::optional<ResponseHead> pointer;

        // A redirect's own body is nothing a caller asked for, so it is swallowed rather than delivered, and a redirect naming nowhere is the answer itself.
        // clang-format off
        const HeadFn answered = [&](ResponseHead& head)
        {
            head.url = hop.url;
            if (request.redirects != RedirectPolicy::Manual && moved(head.status) && !locationOf(head.headers, hop.url).empty())
            {
                pointer = std::move(head);
                return false;
            }

            return transfer.head(std::move(head));
        };
        // clang-format on

        HttpClientPerform::perform(connections, hop, transfer, answered);

        if (!pointer)
        {
            return;
        }

        if (request.redirects == RedirectPolicy::Error)
        {
            throw HttpClientFailure(ErrorCode::Redirect, "[HttpClient] The request was redirected and redirects were refused.");
        }

        if (count == request.maxRedirects)
        {
            throw HttpClientFailure(ErrorCode::Redirect, "[HttpClient] The request was redirected more times than it was allowed to be.");
        }

        const std::string next = locationOf(pointer->headers, hop.url);

        // Credentials belong to the host they were meant for, so they do not travel to another one.
        if (!sameOrigin(hop.url, next))
        {
            RedirectHelpers::erase(hop.headers, "authorization");
            RedirectHelpers::erase(hop.headers, "cookie");
            RedirectHelpers::erase(hop.headers, "proxy-authorization");
        }

        if (RedirectHelpers::becomesGet(pointer->status, hop.method))
        {
            hop.method = "GET";
            hop.withBody = false;
            RedirectHelpers::erase(hop.headers, "content-length");
            RedirectHelpers::erase(hop.headers, "content-type");
        }

        // A body that streams from the caller was read once and cannot be read again for the next hop.
        if (hop.withBody && request.bodySource)
        {
            throw HttpClientFailure(ErrorCode::Redirect, "[HttpClient] The request body streams from \"bodySource\" and cannot be sent again to follow a redirect.");
        }

        hop.url = next;
    }
}

} // namespace varn::http::client
