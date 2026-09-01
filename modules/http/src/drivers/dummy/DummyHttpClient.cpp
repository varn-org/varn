#include "varn/http/HttpClientPerform.h"

#include <stdexcept>

namespace varn::http::client
{

ClientResponse HttpClientPerform::perform(
    const std::string& /*method*/,
    const std::string& /*url*/,
    const std::map<std::string, std::string>& /*headers*/,
    const std::string& /*body*/,
    const ClientRequestOptions& /*options*/)
{
    throw std::runtime_error("[DummyHttpClient] The HTTP client is not available in this build.");
}

void HttpClientPerform::performStream(
    const std::string& /*method*/,
    const std::string& /*url*/,
    const std::map<std::string, std::string>& /*headers*/,
    const std::string& /*body*/,
    const ClientRequestOptions& /*options*/,
    const StreamResponseFn& /*onResponse*/,
    const StreamChunkFn& /*onChunk*/)
{
    throw std::runtime_error("[DummyHttpClient] The HTTP client is not available in this build.");
}

} // namespace varn::http::client
