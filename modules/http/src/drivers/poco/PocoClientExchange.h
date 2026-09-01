#pragma once

#include "varn/http/HttpClientPerform.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>

#include <istream>
#include <map>
#include <string>

#if defined(VARN_ENABLE_TLS)
#include <Poco/Net/Context.h>
#endif

namespace varn::http::client
{

class PocoClientExchange
{
public:
    static ClientResponse performHttp(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options);
    static void performStreamHttp(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options, const StreamResponseFn& onResponse, const StreamChunkFn& onChunk);
#if defined(VARN_ENABLE_TLS)
    static ClientResponse performHttps(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options);
    static void performStreamHttps(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options, const StreamResponseFn& onResponse, const StreamChunkFn& onChunk);
#endif

private:
    static ClientResponse exchange(Poco::Net::HTTPClientSession& session, const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options);
    static void exchangeStream(Poco::Net::HTTPClientSession& session, const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options, const StreamResponseFn& onResponse, const StreamChunkFn& onChunk);
    static bool headerControlSafe(const std::string& value);
    static void applyHeaders(Poco::Net::HTTPRequest& request, const std::map<std::string, std::string>& headers);
    static ResponseHeaders collectHeaders(const Poco::Net::HTTPResponse& response);
    static std::string readResponseBody(std::istream& rs, std::size_t maxBytes);
    static void streamResponseBody(std::istream& rs, std::size_t maxBytes, const StreamChunkFn& onChunk);
#if defined(VARN_ENABLE_TLS)
    static Poco::Net::Context::Ptr tlsClientContext(bool verify);
    static void ensureTlsClientInitialized();
#endif
};

} // namespace varn::http::client
