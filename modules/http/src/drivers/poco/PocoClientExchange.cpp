#include "PocoClientExchange.h"

#include "varn/tls/CaBundle.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Timespan.h>

#include <ostream>
#include <stdexcept>
#include <string>

#if defined(VARN_ENABLE_TLS)
#include <Poco/Crypto/OpenSSLInitializer.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/Net/RejectCertificateHandler.h>
#include <Poco/Net/SSLManager.h>

#include <cstdlib>
#include <filesystem>
#include <mutex>
#endif

namespace varn::http::client
{

ClientResponse PocoClientExchange::exchange(Poco::Net::HTTPClientSession& session, const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options)
{
    const std::string path = uri.getPathAndQuery().empty() ? "/" : uri.getPathAndQuery();
    session.setTimeout(Poco::Timespan(options.timeoutSeconds, 0));

    Poco::Net::HTTPRequest request(method, path, Poco::Net::HTTPMessage::HTTP_1_1);
    applyHeaders(request, headers);
    if (!body.empty())
    {
        request.setContentLength(static_cast<std::streamsize>(body.size()));
    }

    std::ostream& os = session.sendRequest(request);
    if (!body.empty())
    {
        os << body;
    }

    Poco::Net::HTTPResponse response;
    std::istream& rs = session.receiveResponse(response);

    ClientResponse out;
    out.status = static_cast<int>(response.getStatus());
    out.headers = collectHeaders(response);
    out.body = readResponseBody(rs, options.maxResponseBytes);
    return out;
}

void PocoClientExchange::exchangeStream(Poco::Net::HTTPClientSession& session, const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options, const StreamResponseFn& onResponse, const StreamChunkFn& onChunk)
{
    const std::string path = uri.getPathAndQuery().empty() ? "/" : uri.getPathAndQuery();
    session.setTimeout(Poco::Timespan(options.timeoutSeconds, 0));

    Poco::Net::HTTPRequest request(method, path, Poco::Net::HTTPMessage::HTTP_1_1);
    applyHeaders(request, headers);
    if (!body.empty())
    {
        request.setContentLength(static_cast<std::streamsize>(body.size()));
    }

    std::ostream& os = session.sendRequest(request);
    if (!body.empty())
    {
        os << body;
    }

    Poco::Net::HTTPResponse response;
    std::istream& rs = session.receiveResponse(response);

    onResponse(static_cast<int>(response.getStatus()), collectHeaders(response));
    streamResponseBody(rs, options.maxResponseBytes, onChunk);
}

ClientResponse PocoClientExchange::performHttp(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options)
{
    Poco::Net::HTTPClientSession session(uri.getHost(), static_cast<Poco::UInt16>(uri.getPort()));
    return exchange(session, method, uri, headers, body, options);
}

void PocoClientExchange::performStreamHttp(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options, const StreamResponseFn& onResponse, const StreamChunkFn& onChunk)
{
    Poco::Net::HTTPClientSession session(uri.getHost(), static_cast<Poco::UInt16>(uri.getPort()));
    exchangeStream(session, method, uri, headers, body, options, onResponse, onChunk);
}

#if defined(VARN_ENABLE_TLS)
ClientResponse PocoClientExchange::performHttps(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options)
{
    ensureTlsClientInitialized();

    Poco::Net::HTTPSClientSession session(uri.getHost(), static_cast<Poco::UInt16>(uri.getPort()), tlsClientContext(options.verifyTls));
    return exchange(session, method, uri, headers, body, options);
}

void PocoClientExchange::performStreamHttps(const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options, const StreamResponseFn& onResponse, const StreamChunkFn& onChunk)
{
    ensureTlsClientInitialized();

    Poco::Net::HTTPSClientSession session(uri.getHost(), static_cast<Poco::UInt16>(uri.getPort()), tlsClientContext(options.verifyTls));
    exchangeStream(session, method, uri, headers, body, options, onResponse, onChunk);
}
#endif

bool PocoClientExchange::headerControlSafe(const std::string& value)
{
    for (unsigned char c : value)
    {
        if (c < 0x20 || c == 0x7f)
        {
            return false;
        }
    }

    return true;
}

void PocoClientExchange::applyHeaders(Poco::Net::HTTPRequest& request, const std::map<std::string, std::string>& headers)
{
    for (const auto& [name, value] : headers)
    {
        if (name.empty() || !headerControlSafe(name) || !headerControlSafe(value))
        {
            throw std::runtime_error("[PocoClientExchange] A request header name or value contains invalid characters.");
        }

        request.set(name, value);
    }
}

ResponseHeaders PocoClientExchange::collectHeaders(const Poco::Net::HTTPResponse& response)
{
    ResponseHeaders out;
    for (const auto& [name, value] : response)
    {
        out.emplace_back(name, value);
    }

    return out;
}

std::string PocoClientExchange::readResponseBody(std::istream& rs, std::size_t maxBytes)
{
    std::string out;
    char buffer[65536];
    while (rs)
    {
        rs.read(buffer, sizeof(buffer));
        const std::streamsize got = rs.gcount();
        if (got <= 0)
        {
            break;
        }

        if (out.size() + static_cast<std::size_t>(got) > maxBytes)
        {
            throw std::runtime_error("[PocoClientExchange] The response body exceeds the maximum allowed size.");
        }

        out.append(buffer, static_cast<std::size_t>(got));
    }

    return out;
}

void PocoClientExchange::streamResponseBody(std::istream& rs, std::size_t maxBytes, const StreamChunkFn& onChunk)
{
    std::size_t total = 0;
    char buffer[65536];
    while (rs)
    {
        rs.read(buffer, sizeof(buffer));
        const std::streamsize got = rs.gcount();
        if (got <= 0)
        {
            break;
        }

        total += static_cast<std::size_t>(got);
        if (total > maxBytes)
        {
            throw std::runtime_error("[PocoClientExchange] The response body exceeds the maximum allowed size.");
        }

        onChunk(buffer, static_cast<std::size_t>(got));
    }
}

#if defined(VARN_ENABLE_TLS)
Poco::Net::Context::Ptr PocoClientExchange::tlsClientContext(bool verify)
{
#if defined(_WIN32)
    if (verify)
    {
        // Verify against the system root store since the personal store holds no trusted public roots.
        static Poco::Net::Context::Ptr strict = new Poco::Net::Context(
            Poco::Net::Context::TLS_CLIENT_USE, "", Poco::Net::Context::VERIFY_STRICT,
            Poco::Net::Context::OPT_DEFAULTS, Poco::Net::Context::CERT_STORE_ROOT);
        return strict;
    }

    static Poco::Net::Context::Ptr insecure = new Poco::Net::Context(
        Poco::Net::Context::TLS_CLIENT_USE, "", Poco::Net::Context::VERIFY_NONE,
        Poco::Net::Context::OPT_DEFAULTS, Poco::Net::Context::CERT_STORE_ROOT);
    return insecure;
#else
    if (verify)
    {
        const std::string& caBundle = varn::tls::CaBundle::resolve();
        if (caBundle.empty())
        {
            throw std::runtime_error("[PocoClientExchange] No CA trust store was found for certificate verification: " + varn::tls::CaBundle::describeSearch() + ".");
        }

        static Poco::Net::Context::Ptr strict = new Poco::Net::Context(
            Poco::Net::Context::TLS_CLIENT_USE, "", "", caBundle, Poco::Net::Context::VERIFY_STRICT, 9, true,
            "DEFAULT@SECLEVEL=2");
        return strict;
    }

    static Poco::Net::Context::Ptr insecure = new Poco::Net::Context(
        Poco::Net::Context::TLS_CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE, 9, false,
        "DEFAULT@SECLEVEL=2");
    return insecure;
#endif
}

void PocoClientExchange::ensureTlsClientInitialized()
{
    static Poco::Crypto::OpenSSLInitializer sslInitializer;
    static std::once_flag sslOnce;

    // clang-format off
    std::call_once(sslOnce, []
    {
        // The default handler rejects invalid certificates so strict sessions fail closed.
        Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> handler(new Poco::Net::RejectCertificateHandler(false));
        Poco::Net::SSLManager::instance().initializeClient(nullptr, handler, tlsClientContext(true));
    });
    // clang-format on
}
#endif

} // namespace varn::http::client
