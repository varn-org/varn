#include "PocoClientExchange.h"

#include "varn/tls/CaBundle.h"

#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Timespan.h>

#include <chrono>
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

// The one deadline of an exchange, which bounds the connection, the request, the head and every piece of the body, however slowly a server sends them.
class PocoClientExchange::Deadline
{
public:
    explicit Deadline(double seconds)
        : end(std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds)))
    {
    }

    // Lets the next wait of the session last only what is left, and fails the exchange once nothing is.
    void arm(Poco::Net::HTTPClientSession& session) const
    {
        const auto left = std::chrono::duration_cast<std::chrono::microseconds>(end - std::chrono::steady_clock::now()).count();
        if (left <= 0)
        {
            throw Poco::TimeoutException("The request passed its deadline.");
        }

        const Poco::Timespan span(static_cast<Poco::Timespan::TimeDiff>(left));
        if (!session.connected())
        {
            session.setTimeout(span);
            return;
        }

        session.socket().setReceiveTimeout(span);
        session.socket().setSendTimeout(span);
    }

private:
    std::chrono::steady_clock::time_point end;
};

// Closes the session without waiting for the peer to answer the close of TLS, since the exchange is over whether it ended or failed.
class PocoClientExchange::QuietClose
{
public:
    explicit QuietClose(Poco::Net::HTTPClientSession& session)
        : session(session)
    {
    }

    QuietClose(const QuietClose&) = delete;
    QuietClose& operator=(const QuietClose&) = delete;

    ~QuietClose()
    {
        try
        {
            if (session.connected())
            {
                session.socket().setBlocking(false);
            }
        }
        catch (const Poco::Exception&)
        {
        }
    }

private:
    Poco::Net::HTTPClientSession& session;
};

ClientResponse PocoClientExchange::exchange(Poco::Net::HTTPClientSession& session, const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options)
{
    const Deadline deadline(options.timeoutSeconds);
    const QuietClose quiet(session);
    Poco::Net::HTTPResponse response;
    std::istream& rs = open(session, deadline, method, uri, headers, body, response);

    ClientResponse out;
    out.status = static_cast<int>(response.getStatus());
    out.headers = collectHeaders(response);

    // clang-format off
    readPieces(session, rs, deadline, options.maxResponseBytes, [&out](const char* piece, std::size_t length)
    {
        out.body.append(piece, length);
    });
    // clang-format on
    return out;
}

void PocoClientExchange::exchangeStream(Poco::Net::HTTPClientSession& session, const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, const ClientRequestOptions& options, const StreamResponseFn& onResponse, const StreamChunkFn& onChunk)
{
    const Deadline deadline(options.timeoutSeconds);
    const QuietClose quiet(session);
    Poco::Net::HTTPResponse response;
    std::istream& rs = open(session, deadline, method, uri, headers, body, response);

    onResponse(static_cast<int>(response.getStatus()), collectHeaders(response));
    readPieces(session, rs, deadline, options.maxResponseBytes, onChunk);
}

// Sends the request and reads the head of its response within the deadline, and answers the stream of its body.
std::istream& PocoClientExchange::open(Poco::Net::HTTPClientSession& session, const Deadline& deadline, const std::string& method, const Poco::URI& uri, const std::map<std::string, std::string>& headers, const std::string& body, Poco::Net::HTTPResponse& response)
{
    const std::string path = uri.getPathAndQuery().empty() ? "/" : uri.getPathAndQuery();
    Poco::Net::HTTPRequest request(method, path, Poco::Net::HTTPMessage::HTTP_1_1);
    applyHeaders(request, headers);
    if (!body.empty())
    {
        request.setContentLength(static_cast<std::streamsize>(body.size()));
    }

    deadline.arm(session);
    std::ostream& os = session.sendRequest(request);
    if (!body.empty())
    {
        deadline.arm(session);
        os << body;
    }

    deadline.arm(session);
    return session.receiveResponse(response);
}

// Hands each piece of the body over as it arrives, waiting for one byte and then taking what the session already holds, so a stream never waits for a full buffer.
void PocoClientExchange::readPieces(Poco::Net::HTTPClientSession& session, std::istream& rs, const Deadline& deadline, std::size_t maxBytes, const StreamChunkFn& onPiece)
{
    std::size_t total = 0;
    char buffer[65536];

    while (true)
    {
        deadline.arm(session);
        const int first = rs.get();
        if (first == std::char_traits<char>::eof())
        {
            break;
        }

        buffer[0] = static_cast<char>(first);
        const std::size_t got = 1 + static_cast<std::size_t>(rs.readsome(buffer + 1, sizeof(buffer) - 1));
        total += got;
        if (total > maxBytes)
        {
            throw std::runtime_error("[PocoClientExchange] The response body exceeds the maximum allowed size.");
        }

        onPiece(buffer, got);
    }

    // A stream hides the failure of the session behind its end, so a body cut short by a timeout or a lost peer is never taken for a whole one.
    if (const Poco::Exception* failure = session.networkException())
    {
        failure->rethrow();
    }

    if (rs.bad())
    {
        throw std::runtime_error("[PocoClientExchange] The response body ended before it was whole.");
    }
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
