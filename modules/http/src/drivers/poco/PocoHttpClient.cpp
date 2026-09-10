#include "varn/http/HttpClientPerform.h"

#include "PocoClientExchange.h"

#include <Poco/Exception.h>
#include <Poco/URI.h>

#include <map>
#include <stdexcept>
#include <string>

namespace varn::http::client
{

namespace
{
class PocoClientHelpers
{
public:
    static Poco::URI parseUri(const std::string& url)
    {
        Poco::URI uri(url);
        const std::string scheme = uri.getScheme();
        if (scheme != "http" && scheme != "https")
        {
            throw std::runtime_error("[PocoHttpClient] The URL scheme must be \"http\" or \"https\".");
        }

        return uri;
    }

    /// Runs one exchange and turns a failure of Poco into a message that says what went wrong, since its own names such as "Timeout" say little on their own.
    template <typename Exchange>
    static auto described(const Exchange& exchange) -> decltype(exchange())
    {
        try
        {
            return exchange();
        }
        catch (const Poco::TimeoutException&)
        {
            throw std::runtime_error("[PocoHttpClient] The request did not finish within its timeout.");
        }
        catch (const Poco::Exception& failure)
        {
            throw std::runtime_error("[PocoHttpClient] " + failure.displayText());
        }
    }
};
} // namespace

ClientResponse HttpClientPerform::perform(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const ClientRequestOptions& options)
{
    const Poco::URI uri = PocoClientHelpers::parseUri(url);

    if (uri.getScheme() == "https")
    {
#if defined(VARN_ENABLE_TLS)
        // clang-format off
        return PocoClientHelpers::described([&] { return PocoClientExchange::performHttps(method, uri, headers, body, options); });
        // clang-format on
#else
        throw std::runtime_error("[PocoHttpClient] Secure URLs require a build with TLS support enabled.");
#endif
    }

    // clang-format off
    return PocoClientHelpers::described([&] { return PocoClientExchange::performHttp(method, uri, headers, body, options); });
    // clang-format on
}

void HttpClientPerform::performStream(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const ClientRequestOptions& options,
    const StreamResponseFn& onResponse,
    const StreamChunkFn& onChunk)
{
    const Poco::URI uri = PocoClientHelpers::parseUri(url);

    if (uri.getScheme() == "https")
    {
#if defined(VARN_ENABLE_TLS)
        // clang-format off
        PocoClientHelpers::described([&] { PocoClientExchange::performStreamHttps(method, uri, headers, body, options, onResponse, onChunk); });
        // clang-format on
        return;
#else
        throw std::runtime_error("[PocoHttpClient] Secure URLs require a build with TLS support enabled.");
#endif
    }

    // clang-format off
    PocoClientHelpers::described([&] { PocoClientExchange::performStreamHttp(method, uri, headers, body, options, onResponse, onChunk); });
    // clang-format on
}

} // namespace varn::http::client
