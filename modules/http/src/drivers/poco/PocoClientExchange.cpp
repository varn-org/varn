#include "PocoClientExchange.h"

#include "../../HttpClientFailure.h"
#include "../../HttpClientTransfer.h"
#include "HttpClientConnections.h"
#include "varn/tls/CaBundle.h"

#include <Poco/Exception.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/SocketDefs.h>
#include <Poco/Timespan.h>

#include <stdexcept>
#include <string>
#include <utility>

#if defined(VARN_ENABLE_TLS)
#include <Poco/Crypto/OpenSSLInitializer.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/Net/RejectCertificateHandler.h>
#include <Poco/Net/SSLManager.h>

#include <mutex>
#endif

namespace varn::http::client
{

namespace
{
// The pieces a body is read and written in, which is what one receive of a busy connection usually holds.
constexpr std::size_t kPieceBytes = 65536;

// The body of a redirect is read past to keep its connection, unless it is longer than this, when closing is cheaper.
constexpr std::size_t kDrainBytes = 65536;

// The pool closes an idle connection on its own schedule, so Poco is told never to reconnect one behind its back.
constexpr long kNeverReconnectSeconds = 7L * 24L * 60L * 60L;
} // namespace

// The one deadline of a transfer, which bounds the connection, the request, the head and every piece of the body, however slowly a server sends them.
class PocoClientExchange::Deadline
{
public:
    explicit Deadline(std::chrono::steady_clock::time_point end)
        : end(end)
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

// Lets the transfer stop a connection that waits on the network, by shutting the reading side of its socket down from whatever thread ends the transfer.
// The descriptor is captured while the session owns it and the interrupt is cleared before the session can close it, so no other socket is ever touched.
class PocoClientExchange::Interruption
{
public:
    explicit Interruption(HttpClientTransfer& transfer)
        : transfer(transfer)
    {
    }

    Interruption(const Interruption&) = delete;
    Interruption& operator=(const Interruption&) = delete;

    ~Interruption()
    {
        if (armed)
        {
            transfer.clearInterrupt();
        }
    }

    void arm(Poco::Net::HTTPClientSession& session)
    {
        const poco_socket_t descriptor = session.socket().impl()->sockfd();

        // clang-format off
        const bool accepted = transfer.setInterrupt([descriptor]
        {
#if defined(_WIN32)
            ::shutdown(descriptor, SD_BOTH);
#else
            // Only the reading side closes, since a write to a socket closed for writing raises a signal on Linux.
            ::shutdown(descriptor, SHUT_RD);
#endif
        });
        // clang-format on

        if (!accepted)
        {
            throw HttpClientFailure(ErrorCode::Cancelled, "[HttpClient] The request was cancelled.");
        }

        armed = true;
    }

private:
    HttpClientTransfer& transfer;
    bool armed = false;
};

// Runs one hop on a pooled connection when one is open, and once more on a new connection when a pooled one turns out closed before it answered.
void PocoClientExchange::perform(HttpClientConnections& connections, const Poco::URI& uri, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead)
{
    const bool verify = transfer.request().verifyTls;
    const std::string origin = uri.getScheme() + "://" + uri.getHost() + ":" + std::to_string(uri.getPort()) + (verify ? "" : " insecure");

    for (bool mayRetry = true;; mayRetry = false)
    {
        std::unique_ptr<Poco::Net::HTTPClientSession> session = connections.take(origin);
        const bool reused = session != nullptr;
        if (!reused)
        {
            session = open(uri, verify);
        }

        bool answered = false;
        try
        {
            if (exchange(*session, uri, hop, transfer, onHead, answered))
            {
                connections.give(origin, std::move(session));
                return;
            }

            HttpClientConnections::discard(std::move(session));
            return;
        }
        catch (const Poco::Exception&)
        {
            HttpClientConnections::discard(std::move(session));
            if (!mayRetry || !reused || answered || transfer.halted() || !replayable(hop, transfer))
            {
                throw;
            }
        }
        catch (...)
        {
            HttpClientConnections::discard(std::move(session));
            throw;
        }
    }
}

std::unique_ptr<Poco::Net::HTTPClientSession> PocoClientExchange::open(const Poco::URI& uri, bool verifyTls)
{
    std::unique_ptr<Poco::Net::HTTPClientSession> session;

    if (uri.getScheme() == "https")
    {
#if defined(VARN_ENABLE_TLS)
        ensureTlsClientInitialized();
        session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), static_cast<Poco::UInt16>(uri.getPort()), tlsClientContext(verifyTls));
#else
        throw HttpClientFailure(ErrorCode::Unavailable, "[HttpClient] Secure URLs require a build with TLS support enabled.");
#endif
    }
    else
    {
        session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), static_cast<Poco::UInt16>(uri.getPort()));
    }

    session->setKeepAlive(true);
    session->setKeepAliveTimeout(Poco::Timespan(kNeverReconnectSeconds, 0));
    return session;
}

// Sends one request and reads its response, and answers whether the connection is left clean for another request.
bool PocoClientExchange::exchange(Poco::Net::HTTPClientSession& session, const Poco::URI& uri, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead, bool& answered)
{
    if (transfer.halted())
    {
        throw HttpClientFailure(ErrorCode::Cancelled, "[HttpClient] The request was cancelled.");
    }

    const Request& request = transfer.request();
    const Deadline deadline(transfer.deadline());
    const std::string path = uri.getPathAndQuery().empty() ? "/" : uri.getPathAndQuery();
    Poco::Net::HTTPRequest message(hop.method, path, Poco::Net::HTTPMessage::HTTP_1_1);
    for (const auto& [name, value] : hop.headers)
    {
        message.add(name, value);
    }

    const bool streamed = hop.withBody && static_cast<bool>(request.bodySource);
    const bool buffered = hop.withBody && !request.body.empty();
    if (streamed && request.bodyLength)
    {
        message.setContentLength64(static_cast<Poco::Int64>(*request.bodyLength));
    }
    else if (streamed)
    {
        message.setChunkedTransferEncoding(true);
    }
    else if (buffered)
    {
        message.setContentLength64(static_cast<Poco::Int64>(request.body.size()));
    }

    Interruption interruption(transfer);
    deadline.arm(session);
    std::ostream& os = session.sendRequest(message);
    interruption.arm(session);

    if (streamed)
    {
        sendSource(session, os, deadline, transfer);
    }
    else if (buffered)
    {
        deadline.arm(session);
        os.write(request.body.data(), static_cast<std::streamsize>(request.body.size()));
    }

    deadline.arm(session);
    Poco::Net::HTTPResponse response;
    std::istream& rs = session.receiveResponse(response);
    answered = true;

    const int status = static_cast<int>(response.getStatus());
    const bool bodiless = hop.method == "HEAD" || status < 200 || status == 204 || status == 304;

    ResponseHead head;
    head.status = status;
    head.headers = collectHeaders(response);
    if (bodiless)
    {
        head.contentLength = 0;
    }
    else if (response.hasContentLength() && !response.getChunkedTransferEncoding())
    {
        head.contentLength = static_cast<std::uint64_t>(response.getContentLength64());
    }

    if (!onHead(head))
    {
        return !transfer.halted() && drain(session, rs, deadline) && response.getKeepAlive();
    }

    return readBody(session, rs, deadline, transfer) && response.getKeepAlive();
}

// Writes a body that streams from the caller, piece by piece within the deadline.
void PocoClientExchange::sendSource(Poco::Net::HTTPClientSession& session, std::ostream& os, const Deadline& deadline, HttpClientTransfer& transfer)
{
    char buffer[kPieceBytes];

    while (os.good())
    {
        const std::size_t written = transfer.readBody(buffer, sizeof(buffer));
        if (written == 0)
        {
            return;
        }

        deadline.arm(session);
        os.write(buffer, static_cast<std::streamsize>(written));
    }
}

// Hands each piece of the body over as it arrives, waiting for one byte and then taking what the session already holds, so a stream never waits for a full buffer.
// Answers whether the body was read whole, which is false once the transfer stopped wanting it.
bool PocoClientExchange::readBody(Poco::Net::HTTPClientSession& session, std::istream& rs, const Deadline& deadline, HttpClientTransfer& transfer)
{
    char buffer[kPieceBytes];

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
        if (!transfer.deliver(buffer, got))
        {
            return false;
        }
    }

    if (transfer.halted())
    {
        return false;
    }

    // A stream hides the failure of the session behind its end, so a body cut short by a timeout or a lost peer is never taken for a whole one.
    if (const Poco::Exception* failure = session.networkException())
    {
        failure->rethrow();
    }

    if (rs.bad())
    {
        throw HttpClientFailure(ErrorCode::Network, "[HttpClient] The response body ended before it was whole.");
    }

    return true;
}

// Reads past a short body nobody wants, and answers whether it ended so the connection can carry another request.
bool PocoClientExchange::drain(Poco::Net::HTTPClientSession& session, std::istream& rs, const Deadline& deadline)
{
    char buffer[kPieceBytes];
    std::size_t skipped = 0;

    while (skipped <= kDrainBytes)
    {
        deadline.arm(session);
        rs.read(buffer, sizeof(buffer));
        skipped += static_cast<std::size_t>(rs.gcount());
        if (rs.eof())
        {
            return session.networkException() == nullptr && !rs.bad();
        }

        if (!rs.good())
        {
            return false;
        }
    }

    return false;
}

// A request may be sent again on a new connection only when doing it twice means the same as doing it once and its body can be read again.
bool PocoClientExchange::replayable(const Hop& hop, const HttpClientTransfer& transfer)
{
    if (hop.withBody && transfer.request().bodySource)
    {
        return false;
    }

    return hop.method == "GET" || hop.method == "HEAD" || hop.method == "OPTIONS" || hop.method == "TRACE" || hop.method == "PUT" || hop.method == "DELETE";
}

Headers PocoClientExchange::collectHeaders(const Poco::Net::HTTPResponse& response)
{
    Headers out;
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
            throw HttpClientFailure(ErrorCode::Tls, "[HttpClient] No CA trust store was found for certificate verification: " + varn::tls::CaBundle::describeSearch() + ".");
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
