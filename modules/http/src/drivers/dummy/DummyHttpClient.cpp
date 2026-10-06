#include "../../HttpClientPerform.h"

#include "../../HttpClientFailure.h"

namespace varn::http::client
{

// A build without a transport keeps no connections.
class HttpClientConnections
{
};

std::shared_ptr<HttpClientConnections> HttpClientPerform::connections()
{
    return std::make_shared<HttpClientConnections>();
}

void HttpClientPerform::perform(HttpClientConnections& /*connections*/, const Hop& /*hop*/, HttpClientTransfer& /*transfer*/, const HeadFn& /*onHead*/)
{
    throw HttpClientFailure(ErrorCode::Unavailable, "[DummyHttpClient] The HTTP client is not available in this build.");
}

} // namespace varn::http::client
