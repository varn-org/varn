#include "varn/http/HttpClientPerform.h"

#include "PocoClientExchange.h"

#include <Poco/URI.h>

#include <map>
#include <stdexcept>
#include <string>

namespace varn::http::client
{

namespace
{
class HttpUrlHelpers
{
public:
    static Poco::URI parseUri(const std::string& url)
    {
        Poco::URI uri(url);
        const std::string scheme = uri.getScheme();
        if (scheme != "http" && scheme != "https")
        {
            throw std::runtime_error("[PocoHttpClient] The URL scheme must be http or https.");
        }

        return uri;
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
    const Poco::URI uri = HttpUrlHelpers::parseUri(url);

    if (uri.getScheme() == "https")
    {
#if defined(VARN_ENABLE_TLS)
        return PocoClientExchange::performHttps(method, uri, headers, body, options);
#else
        throw std::runtime_error("[PocoHttpClient] Secure URLs require a build with TLS support enabled.");
#endif
    }

    return PocoClientExchange::performHttp(method, uri, headers, body, options);
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
    const Poco::URI uri = HttpUrlHelpers::parseUri(url);

    if (uri.getScheme() == "https")
    {
#if defined(VARN_ENABLE_TLS)
        PocoClientExchange::performStreamHttps(method, uri, headers, body, options, onResponse, onChunk);
        return;
#else
        throw std::runtime_error("[PocoHttpClient] Secure URLs require a build with TLS support enabled.");
#endif
    }

    PocoClientExchange::performStreamHttp(method, uri, headers, body, options, onResponse, onChunk);
}

} // namespace varn::http::client
