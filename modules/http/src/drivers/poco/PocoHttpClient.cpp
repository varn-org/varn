#include "../../HttpClientPerform.h"

#include "../../HttpClientFailure.h"
#include "../../HttpClientTransfer.h"
#include "HttpClientConnections.h"
#include "PocoClientExchange.h"

#include <Poco/Exception.h>
#include <Poco/URI.h>

#if defined(VARN_ENABLE_TLS)
#include <Poco/Net/SSLException.h>
#endif

#include <string>

namespace varn::http::client
{

namespace
{
class PocoClientHelpers
{
public:
    PocoClientHelpers() = delete;

    static Poco::URI parseUri(const std::string& url)
    {
        try
        {
            return Poco::URI(url);
        }
        catch (const Poco::Exception& failure)
        {
            throw HttpClientFailure(ErrorCode::Invalid, "The URL \"" + url + "\" could not be parsed: " + failure.displayText());
        }
    }

    /// Runs one exchange and turns a failure of Poco into the kind it is and a message that says what went wrong, since its own names such as "Timeout" say little on their own.
    template <typename Exchange>
    static void described(HttpClientTransfer& transfer, const Exchange& exchange)
    {
        try
        {
            exchange();
        }
        catch (const Poco::TimeoutException&)
        {
            throw HttpClientFailure(ErrorCode::Timeout, transfer.timeoutError().message);
        }
#if defined(VARN_ENABLE_TLS)
        catch (const Poco::Net::SSLException& failure)
        {
            throw HttpClientFailure(ErrorCode::Tls, "The secure connection failed (" + failure.displayText() + ").");
        }
#endif
        catch (const Poco::SyntaxException& failure)
        {
            throw HttpClientFailure(ErrorCode::Invalid, "The request is not valid (" + failure.displayText() + ").");
        }
        catch (const Poco::Exception& failure)
        {
            throw HttpClientFailure(ErrorCode::Network, "The request could not reach the server (" + failure.displayText() + ").");
        }
    }
};
} // namespace

std::shared_ptr<HttpClientConnections> HttpClientPerform::connections()
{
    return std::make_shared<HttpClientConnections>();
}

void HttpClientPerform::perform(HttpClientConnections& connections, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead)
{
    const Poco::URI uri = PocoClientHelpers::parseUri(hop.url);
    if (uri.getScheme() != "http" && uri.getScheme() != "https")
    {
        throw HttpClientFailure(ErrorCode::Invalid, "The URL \"" + hop.url + "\" must be an \"http\" or \"https\" address.");
    }

    // clang-format off
    PocoClientHelpers::described(transfer, [&] { PocoClientExchange::perform(connections, uri, hop, transfer, onHead); });
    // clang-format on
}

} // namespace varn::http::client
