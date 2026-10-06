#include "PocoClientExchange.h"

#include "../../HttpClientFailure.h"
#include "../../HttpClientTransfer.h"
#include "HttpClientConnections.h"

#include <Poco/Exception.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/SocketDefs.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Timespan.h>

#if !defined(_WIN32)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <stdexcept>
#include <string>
#include <utility>

#if defined(VARN_ENABLE_TLS)
#include <Poco/Crypto/OpenSSLInitializer.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/Net/RejectCertificateHandler.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/SecureStreamSocket.h>

#include "varn/tls/SystemTrust.h"

#include <exception>
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

// A session whose socket the exchange connects itself, so the interruption of its transfer is armed before anything waits on the network.
template <typename Session>
class ConnectedSession final : public Session
{
public:
    using Session::attachSocket;
    using Session::Session;
};
} // namespace

// The one deadline of a transfer, which bounds the connection, the request, the head and every piece of the body, however slowly a server sends them.
class PocoClientExchange::Deadline
{
public:
    explicit Deadline(std::chrono::steady_clock::time_point end)
        : end(end)
    {
    }

    // Answers what is left of the deadline, and fails the exchange once nothing is.
    Poco::Timespan left() const
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(end - std::chrono::steady_clock::now()).count();
        if (remaining <= 0)
        {
            throw Poco::TimeoutException("The request passed its deadline.");
        }

        return Poco::Timespan(static_cast<Poco::Timespan::TimeDiff>(remaining));
    }

    // Lets the next wait of the socket last only what is left.
    void arm(Poco::Net::Socket& socket) const
    {
        const Poco::Timespan span = left();
        socket.setReceiveTimeout(span);
        socket.setSendTimeout(span);
    }

    // Lets the next wait of the session last only what is left, a connection the session opens again on its own included.
    void arm(Poco::Net::HTTPClientSession& session) const
    {
        const Poco::Timespan span = left();
        session.setTimeout(span);
        if (session.connected())
        {
            session.socket().setReceiveTimeout(span);
            session.socket().setSendTimeout(span);
        }
    }

private:
    std::chrono::steady_clock::time_point end;
};

// Lets the transfer stop a connection that waits on the network, by shutting its socket down from whatever thread ends the transfer.
// On POSIX systems the shutdown goes through a descriptor of its own for the socket, closed only once the interrupt is cleared, so a session that closes its socket on a failure never leaves a number that another socket may take before the interrupt runs.
// A shutdown through a duplicate of a socket of Windows leaves its connection open, so there the shutdown goes through the descriptor of the socket itself.
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
        clear();
    }

    void arm(const Poco::Net::Socket& socket)
    {
#if defined(_WIN32)
        target = socket.impl()->sockfd();
#else
        target = duplicate(socket.impl()->sockfd());
#endif
        const poco_socket_t descriptor = target;

        // clang-format off
        const bool accepted = transfer.setInterrupt([descriptor]
        {
            shutdown(descriptor);
        });
        // clang-format on

        if (!accepted)
        {
            throw HttpClientFailure(ErrorCode::Cancelled, "The request was cancelled.");
        }
    }

    // Takes the interrupt back before the descriptor closes, so no interrupt reaches the socket once the session may carry another transfer.
    void clear()
    {
        if (target == POCO_INVALID_SOCKET)
        {
            return;
        }

        transfer.clearInterrupt();
#if !defined(_WIN32)
        ::close(target);
#endif
        target = POCO_INVALID_SOCKET;
    }

private:
#if !defined(_WIN32)
    static poco_socket_t duplicate(poco_socket_t descriptor)
    {
        const poco_socket_t duplicated = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
        if (duplicated == POCO_INVALID_SOCKET)
        {
            throw HttpClientFailure(ErrorCode::Network, "The connection could not be opened, because its socket could not be duplicated (system error " + std::to_string(errno) + ").");
        }

        return duplicated;
    }
#endif

    static void shutdown(poco_socket_t descriptor)
    {
#if defined(_WIN32)
        ::shutdown(descriptor, SD_BOTH);
#elif defined(__APPLE__) || defined(__FreeBSD__)
        // Poco sets "SO_NOSIGPIPE" on every socket it makes here, so both sides close and a write that waits on the peer ends as well.
        ::shutdown(descriptor, SHUT_RDWR);
#else
        // A write to a socket shut for writing raises a signal here, so only the reading side closes and a write that waits on the peer ends at the deadline.
        ::shutdown(descriptor, SHUT_RD);
#endif
    }

    HttpClientTransfer& transfer;
    poco_socket_t target = POCO_INVALID_SOCKET;
};

// Runs one hop on a pooled connection when one is open, and once more on a new connection when a pooled one turns out closed before it answered.
void PocoClientExchange::perform(HttpClientConnections& connections, const Poco::URI& uri, const Hop& hop, HttpClientTransfer& transfer, const HeadFn& onHead)
{
    const bool verify = transfer.request().verifyTls;
    const std::string origin = uri.getScheme() + "://" + uri.getHost() + ":" + std::to_string(uri.getPort()) + (verify ? "" : " insecure");

    for (bool mayRetry = true;; mayRetry = false)
    {
        Interruption interruption(transfer);
        std::unique_ptr<Poco::Net::HTTPClientSession> session = connections.take(origin);
        const bool reused = session != nullptr;
        bool answered = false;

        try
        {
            if (reused)
            {
                interruption.arm(session->socket());
            }
            else
            {
                session = open(uri, verify, Deadline(transfer.deadline()), interruption);
            }

            const bool clean = exchange(*session, uri, hop, transfer, onHead, answered);
            interruption.clear();

            // A transfer that halted as its exchange ended may have shut the socket down, so only a connection no interrupt reached goes back to the pool.
            if (clean && !transfer.halted())
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

// Connects to the origin of the URL with the interruption already armed on the socket, so a cancel reaches the connect and the TLS handshake too wherever the system lets a shutdown end them.
std::unique_ptr<Poco::Net::HTTPClientSession> PocoClientExchange::open(const Poco::URI& uri, bool verifyTls, const Deadline& deadline, Interruption& interruption)
{
    const bool secure = uri.getScheme() == "https";
#if !defined(VARN_ENABLE_TLS)
    if (secure)
    {
        throw HttpClientFailure(ErrorCode::Unavailable, "Secure URLs require a build with TLS support enabled.");
    }
#endif

    const std::string& host = uri.getHost();
    const auto port = static_cast<Poco::UInt16>(uri.getPort());
    const Poco::Net::SocketAddress address(host, port);
    Poco::Net::StreamSocket socket(address.family());
    interruption.arm(socket);
    socket.connect(address, deadline.left());
    socket.setNoDelay(true);

    std::unique_ptr<Poco::Net::HTTPClientSession> session;
#if defined(VARN_ENABLE_TLS)
    if (secure)
    {
        ensureTlsClientInitialized();
        const Poco::Net::Context::Ptr context = tlsClientContext(verifyTls);
        deadline.arm(socket);
        auto opened = std::make_unique<ConnectedSession<Poco::Net::HTTPSClientSession>>(host, port, context);
        opened->attachSocket(Poco::Net::SecureStreamSocket::attach(socket, host, context));
        session = std::move(opened);
    }
#endif

    if (!session)
    {
        auto opened = std::make_unique<ConnectedSession<Poco::Net::HTTPClientSession>>(host, port);
        opened->attachSocket(socket);
        session = std::move(opened);
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
        throw HttpClientFailure(ErrorCode::Cancelled, "The request was cancelled.");
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

    deadline.arm(session);
    std::ostream& os = session.sendRequest(message);

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
        throw HttpClientFailure(ErrorCode::Network, "The response body ended before it was whole.");
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
    if (verify)
    {
        static const Poco::Net::Context::Ptr strict = createTlsClientContext(true);
        return strict;
    }

    static const Poco::Net::Context::Ptr insecure = createTlsClientContext(false);
    return insecure;
}

Poco::Net::Context::Ptr PocoClientExchange::createTlsClientContext(bool verify)
{
    Poco::Net::Context::Ptr context = new Poco::Net::Context(
        Poco::Net::Context::TLS_CLIENT_USE, "", "", "", verify ? Poco::Net::Context::VERIFY_STRICT : Poco::Net::Context::VERIFY_NONE, 9, false,
        "DEFAULT@SECLEVEL=2");

    if (!verify)
    {
        return context;
    }

    try
    {
        varn::tls::SystemTrust::install(context->sslContext());
    }
    catch (const std::exception& ex)
    {
        throw HttpClientFailure(ErrorCode::Tls, ex.what());
    }

    return context;
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
