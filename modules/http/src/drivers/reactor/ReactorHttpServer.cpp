#include "varn/http/drivers/reactor/ReactorHttpServer.h"

#include "../../HttpText.h"
#include "varn/http/StaticFileHandler.h"
#include "varn/log/Log.h"
#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"

#include <Poco/Base64Encoder.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/SocketDefs.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/SHA1Engine.h>
#include <Poco/String.h>

#include <cstring>
#include <llhttp.h>

#if defined(VARN_HAVE_ZLIB)
#include <zlib.h>
#endif

#ifdef VARN_ENABLE_TLS
#include "../poco/TlsServerContext.h"
#include <Poco/Net/SecureServerSocket.h>
#endif

#if defined(__linux__)
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#endif

#if defined(__APPLE__)
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <climits>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace varn::http
{

using varn::runtime::EventLoop;
using varn::runtime::Runtime;

namespace
{

constexpr std::size_t kMaxHeaderBytes = 64 * 1024;
constexpr std::size_t kMaxChunkLineBytes = 8 * 1024;
constexpr int kReadChunk = 65536;
constexpr std::size_t kReadPassBytes = 1024 * 1024;
constexpr std::size_t kFileChunkBytes = 64 * 1024;
constexpr long long kSweepIntervalMs = 1000;

// These mirror `Poco::Net::SecureStreamSocket::ERR_SSL_WANT_READ` and `ERR_SSL_WANT_WRITE`, so TLS I/O needs no SSL header here.
constexpr int kSslWantRead = -2;
constexpr int kSslWantWrite = -3;

#if (defined(__linux__) || defined(__APPLE__)) && !defined(VARN_NO_SENDFILE)
#define VARN_HAS_SENDFILE 1
#endif

#if defined(VARN_HAS_SENDFILE)

enum class FileSend
{
    Progress,
    WouldBlock,
    Eof,
    Error
};

#endif

constexpr std::size_t kCompressThreshold = 1024;

constexpr int kWsContinuation = 0x0;
constexpr int kWsText = 0x1;
constexpr int kWsBinary = 0x2;
constexpr int kWsClose = 0x8;
constexpr int kWsPing = 0x9;
constexpr int kWsPong = 0xA;
constexpr int kWsCloseGoingAway = 1001;
constexpr int kWsCloseProtocolError = 1002;
constexpr int kWsCloseNoStatus = 1005;
constexpr int kWsCloseAbnormal = 1006;
constexpr int kWsCloseInvalidData = 1007;
constexpr int kWsCloseTooLarge = 1009;
constexpr std::size_t kRejectDrainBytes = 1024 * 1024;
constexpr long long kRejectDrainMs = 2000;
constexpr std::size_t kStreamMaxOutBytes = 16 * 1024 * 1024;
constexpr std::size_t kMaxGatherSegments = 64;
constexpr std::size_t kInlineChunkBytes = 16 * 1024;

constexpr const char* kResponseEnded = "[HttpServer] The response already ended.";
constexpr const char* kClientGone = "[HttpServer] The client closed the connection before the data was sent.";
constexpr const char* kClientStalled = "[HttpServer] The client stopped reading while too much data waited for it, so the connection was dropped.";
constexpr const char* kPastContentLength = "[HttpServer] The write goes past the \"Content-Length\" the handler set.";
constexpr const char* kWsStalled = "The peer stopped reading while too much data waited for it.";
constexpr const char* kWsNoPong = "The peer did not answer a ping in time.";

struct WsFrame
{
    bool fin = false;
    int opcode = 0;
    std::string payload;
    std::size_t consumed = 0;
};

enum class WsParse
{
    Ok,
    NeedMore,
    TooLarge,
    Error
};

class HttpConnection;

} // namespace

// The state the listener, the sweep and every connection of one server share, so a close learns when the last connection it waits for is gone.
class ReactorServerState
{
public:
    void connectionClosed(EventLoop& loop);
    void settleIfDrained(EventLoop& loop);
    std::vector<std::shared_ptr<HttpConnection>> liveConnections() const;

    std::atomic<bool> stopping{false};
    std::atomic<bool> forced{false};
    bool closing = false;
    std::vector<std::weak_ptr<HttpConnection>> registry;
    std::size_t open = 0;
    std::vector<std::function<void()>> closeWaiters;
    EventLoop::TimerId closeDeadline = 0;
    EventLoop::TimerId sweepTimer = 0;
};

namespace
{

class ReactorHelpers
{
public:
#if defined(VARN_HAS_SENDFILE)
    static FileSend sendFileToSocket(int socketFd, int fileFd, std::uint64_t offset, std::size_t count, std::size_t& sent)
    {
        // Hand up to `count` bytes from the open file straight to the socket without copying through user space, reporting the bytes moved in `sent`.
        sent = 0;

#if defined(__linux__)
        off_t cursor = static_cast<off_t>(offset);
        const ssize_t moved = ::sendfile(socketFd, fileFd, &cursor, count);

        if (moved > 0)
        {
            sent = static_cast<std::size_t>(moved);
            return FileSend::Progress;
        }

        if (moved == 0)
        {
            return FileSend::Eof;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return FileSend::WouldBlock;
        }

        return FileSend::Error;
#else
        off_t length = static_cast<off_t>(count);
        const int rc = ::sendfile(fileFd, socketFd, static_cast<off_t>(offset), &length, nullptr, 0);
        sent = static_cast<std::size_t>(length);

        if (rc == 0)
        {
            return length > 0 ? FileSend::Progress : FileSend::Eof;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return length > 0 ? FileSend::Progress : FileSend::WouldBlock;
        }

        return FileSend::Error;
#endif
    }
#endif

    static long long nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    static const char* reasonPhrase(int code)
    {
        switch (code)
        {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 202:
            return "Accepted";
        case 204:
            return "No Content";
        case 206:
            return "Partial Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 303:
            return "See Other";
        case 304:
            return "Not Modified";
        case 307:
            return "Temporary Redirect";
        case 308:
            return "Permanent Redirect";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 408:
            return "Request Timeout";
        case 413:
            return "Payload Too Large";
        case 429:
            return "Too Many Requests";
        case 500:
            return "Internal Server Error";
        case 502:
            return "Bad Gateway";
        case 503:
            return "Service Unavailable";
        case 504:
            return "Gateway Timeout";
        default:
            // Leave the reason phrase empty for an unlisted code since HTTP allows it and inventing "OK" would misdescribe the status.
            return "";
        }
    }

    static std::string sanitizeHeader(const std::string& value)
    {
        std::string out;
        out.reserve(value.size());
        for (char c : value)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            if (u >= 0x20 && u != 0x7f)
            {
                out += c;
            }
        }

        return out;
    }

#if defined(VARN_HAVE_ZLIB)
    static bool gzipEncode(const std::string& input, std::string& output)
    {
        // Gzip a body with the standard `16+MAX_WBITS` window so the output carries a gzip header rather than raw deflate.
        z_stream stream{};
        if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        {
            return false;
        }

        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
        stream.avail_in = static_cast<uInt>(input.size());

        output.clear();
        output.resize(deflateBound(&stream, static_cast<uLong>(input.size())));

        stream.next_out = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        const int result = deflate(&stream, Z_FINISH);
        deflateEnd(&stream);

        if (result != Z_STREAM_END)
        {
            return false;
        }

        output.resize(stream.total_out);
        return true;
    }

    static bool compressibleType(const std::string& contentType)
    {
        // JSON, XML and any `text/*` payload compress well, whereas already-encoded media gains nothing.
        std::string lowered = contentType;
        Poco::toLowerInPlace(lowered);
        return lowered.rfind("text/", 0) == 0 || lowered.rfind("application/json", 0) == 0 || lowered.rfind("application/xml", 0) == 0;
    }

    static bool acceptsGzip(const std::string& acceptEncoding)
    {
        std::string lowered = acceptEncoding;
        Poco::toLowerInPlace(lowered);
        return lowered.find("gzip") != std::string::npos;
    }
#endif

    static WsParse parseWsFrame(const std::string& buffer, std::size_t maxMessageBytes, WsFrame& frame)
    {
        const std::size_t size = buffer.size();
        if (size < 2)
        {
            return WsParse::NeedMore;
        }

        const auto byteAt = [&](std::size_t i)
        { return static_cast<unsigned char>(buffer[i]); };
        frame.fin = (byteAt(0) & 0x80) != 0;

        // No extension is negotiated, so any reserved bit set is a protocol error.
        if ((byteAt(0) & 0x70) != 0)
        {
            return WsParse::Error;
        }

        frame.opcode = byteAt(0) & 0x0F;
        const bool masked = (byteAt(1) & 0x80) != 0;

        std::uint64_t length = byteAt(1) & 0x7F;
        std::size_t header = 2;
        if (length == 126)
        {
            if (size < 4)
            {
                return WsParse::NeedMore;
            }

            length = (static_cast<std::uint64_t>(byteAt(2)) << 8) | byteAt(3);
            header = 4;
        }
        else if (length == 127)
        {
            if (size < 10)
            {
                return WsParse::NeedMore;
            }

            length = 0;
            for (std::size_t i = 0; i < 8; ++i)
            {
                length = (length << 8) | byteAt(2 + i);
            }

            header = 10;
        }

        if (!masked)
        {
            return WsParse::Error;
        }

        if (length > maxMessageBytes)
        {
            return WsParse::TooLarge;
        }

        if (size < header + 4 + length)
        {
            return WsParse::NeedMore;
        }

        const std::size_t maskOffset = header;
        const std::size_t dataOffset = header + 4;
        frame.payload.resize(static_cast<std::size_t>(length));
        for (std::size_t i = 0; i < length; ++i)
        {
            frame.payload[i] = static_cast<char>(byteAt(dataOffset + i) ^ byteAt(maskOffset + (i & 3)));
        }

        frame.consumed = dataOffset + static_cast<std::size_t>(length);
        return WsParse::Ok;
    }

    // Builds the head of a final frame that carries a payload of the length, which the caller sends behind it.
    static std::string wsHeader(int opcode, std::size_t length)
    {
        std::string out;
        out.reserve(10);
        out.push_back(static_cast<char>(0x80 | (opcode & 0x0F)));

        if (length < 126)
        {
            out.push_back(static_cast<char>(length));
        }
        else if (length <= 0xFFFF)
        {
            out.push_back(static_cast<char>(126));
            out.push_back(static_cast<char>((length >> 8) & 0xFF));
            out.push_back(static_cast<char>(length & 0xFF));
        }
        else
        {
            out.push_back(static_cast<char>(127));
            for (int shift = 56; shift >= 0; shift -= 8)
            {
                out.push_back(static_cast<char>((static_cast<std::uint64_t>(length) >> shift) & 0xFF));
            }
        }

        return out;
    }

    static std::string wsClosePayload(int code, const std::string& reason)
    {
        std::string payload;
        payload.reserve(2 + reason.size());
        payload.push_back(static_cast<char>((code >> 8) & 0xFF));
        payload.push_back(static_cast<char>(code & 0xFF));
        payload += reason;
        return payload;
    }

    static std::string computeWsAccept(const std::string& key)
    {
        Poco::SHA1Engine sha1;
        sha1.update(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        const Poco::DigestEngine::Digest& digest = sha1.digest();

        std::ostringstream encoded;
        Poco::Base64Encoder base64(encoded);
        base64.write(reinterpret_cast<const char*>(digest.data()), static_cast<std::streamsize>(digest.size()));
        base64.close();

        std::string accept = encoded.str();
        accept.erase(std::remove(accept.begin(), accept.end(), '\n'), accept.end());
        accept.erase(std::remove(accept.begin(), accept.end(), '\r'), accept.end());
        return accept;
    }

    static void scheduleSweep(EventLoop& loop, std::shared_ptr<ReactorServerState> state, long long timeoutMs);
};

// The response is filled by the Lua handler on the loop thread, and its first write sends the head and streams every write after it.
class ReactorResponse final : public HttpResponse, public std::enable_shared_from_this<ReactorResponse>
{
public:
    ReactorResponse(std::shared_ptr<HttpConnection> conn, EventLoop& loop, bool keepAlive, bool headersOnly, bool gzipAllowed)
        : connection(std::move(conn))
        , loop(loop)
        , keepAlive(keepAlive)
        , headersOnly(headersOnly)
        , gzipAllowed(gzipAllowed)
    {
    }

    ~ReactorResponse() override
    {
        cancelDeadline();
    }

    void setStatus(int code) override
    {
        if (!finished && !streaming)
        {
            statusCode = code;
        }
    }

    void setHeader(const std::string& name, const std::string& value) override
    {
        if (!finished && !streaming)
        {
            headerMap[ReactorHelpers::sanitizeHeader(name)] = ReactorHelpers::sanitizeHeader(value);
        }
    }

    void addHeader(const std::string& name, const std::string& value) override
    {
        if (!finished && !streaming)
        {
            extraHeaders.emplace_back(ReactorHelpers::sanitizeHeader(name), ReactorHelpers::sanitizeHeader(value));
        }
    }

    void write(std::string chunk, WriteDone done) override;
    void end(std::string body) override;
    void fail() override;
    void sendFile(const std::string& path, std::uint64_t start, std::uint64_t length, bool headersOnly) override;
    void armDeadline(long long timeoutMs);

    bool ended() const override
    {
        return finished;
    }

private:
    enum class Framing
    {
        Length,
        Declared,
        Chunked,
        None
    };

    std::string buildHead(Framing framing, std::size_t contentLength);
    std::vector<std::string> startStream();
    bool frame(std::string chunk, std::vector<std::string>& segments);
    void maybeCompress(std::string& body);
    void expire();
    void cancelDeadline();

    std::shared_ptr<HttpConnection> connection;
    EventLoop& loop;
    EventLoop::TimerId deadline = 0;
    bool keepAlive;
    bool headersOnly;
    bool gzipAllowed;
    bool finished = false;
    bool streaming = false;
    bool chunkedMode = false;
    bool discardBody = false;
    std::uint64_t declaredRemaining = 0;
    int statusCode = 200;
    std::map<std::string, std::string> headerMap;
    std::vector<std::pair<std::string, std::string>> extraHeaders;
};

// One connection state machine, multiplexed on the reactor I/O thread and kept alive by the handlers and the response that reference it.
class HttpConnection final : public std::enable_shared_from_this<HttpConnection>, public WebSocketConnection
{
public:
    HttpConnection(Poco::Net::StreamSocket socket, EventLoop& loop, Runtime& runtime, HttpServerOptions opts, HttpHandler handler, WebSocketHandler onWebSocket, std::shared_ptr<StaticFileHandler> staticFiles, std::shared_ptr<ReactorServerState> server)
        : socket(std::move(socket))
        , loop(loop)
        , runtime(runtime)
        , options(std::move(opts))
        , handler(std::move(handler))
        , onWebSocket(std::move(onWebSocket))
        , staticFiles(std::move(staticFiles))
        , server(std::move(server))
    {
        this->socket.setBlocking(false);

        try
        {
            this->socket.setNoDelay(true);
        }
        catch (...)
        {
        }

        try
        {
            remoteAddress = this->socket.peerAddress().toString();
        }
        catch (...)
        {
        }

        requestStartMs = ReactorHelpers::nowMs();
        llhttp_init(&parser, HTTP_REQUEST, requestSettings());
    }

    // A connection that nothing refers to any more ends here, and it still counts as closed for a server that waits for it, unless the runtime is tearing the server down.
    ~HttpConnection()
    {
        closeFileFd();
        if (!closed && !server->forced.load(std::memory_order_acquire))
        {
            closed = true;
            server->connectionClosed(loop);
        }
    }

    void start()
    {
        armRead();
    }

    bool isClosed() const
    {
        return closed;
    }

    bool isStale(long long now, long long timeoutMs) const
    {
        if (!dispatched && outQueue.empty() && (now - requestStartMs) > timeoutMs)
        {
            return true;
        }

        // A response that has stopped draining for the timeout is a slow-read client holding the slot open.
        return !outQueue.empty() && (now - lastWriteMs) > timeoutMs;
    }

    void forceClose()
    {
        closeNow();
    }

    // Ends the connection for a server that closes: a WebSocket says it goes away, an idle connection closes now, and a request in flight finishes first when the close is graceful.
    void shutdown(bool graceful)
    {
        if (closed)
        {
            return;
        }

        if (wsMode)
        {
            close(kWsCloseGoingAway, std::string());
            if (graceful)
            {
                return;
            }

            // The close frame leaves now when the socket takes it, since the connection ends right after.
            std::ignore = wsOnWritable();
            closeNow();
            return;
        }

        if (graceful && (dispatched || draining || !outQueue.empty()))
        {
            return;
        }

        closeNow();
    }

    // Sends the head and the body as two buffers, so a large body is never copied behind its head.
    void submitResponse(std::string head, std::string body, bool keepAliveAfter)
    {
        // Drop the response if the connection was already force-closed while the handler was still awaiting.
        if (closed)
        {
            return;
        }

        queueOut(std::move(head));
        queueOut(std::move(body));
        keepAlive = keepAliveAfter;
        armWrite();
    }

    // Queues the buffers of one streamed write and calls `done` once they left for the socket, or with the reason they never will.
    void submitStream(std::vector<std::string> segments, HttpResponse::WriteDone done, bool keepAliveAfter, bool last)
    {
        if (closed)
        {
            if (done)
            {
                done(kClientGone);
            }

            return;
        }

        std::uint64_t size = 0;
        for (const std::string& segment : segments)
        {
            size += segment.size();
        }

        // Drop a peer that has stopped draining, so writes that nobody awaits cannot grow the backlog without bound, while one large write onto a drained connection still goes out.
        const std::uint64_t backlog = outQueued - outSent;
        if (backlog > 0 && backlog + size > kStreamMaxOutBytes)
        {
            failFlushes(kClientStalled);
            closeNow();
            if (done)
            {
                done(kClientStalled);
            }

            return;
        }

        for (std::string& segment : segments)
        {
            queueOut(std::move(segment));
        }

        if (done)
        {
            flushWaiters.push_back({outQueued, 0, std::move(done)});
        }

        streamingActive = !last;
        keepAlive = keepAliveAfter;

        // A writer is already draining the queue, so the new buffers ride along without a second queued handler.
        if (streamWriteArmed)
        {
            return;
        }

        streamWriteArmed = true;
        armWrite();
    }

    void streamFile(std::string head, std::string path, std::uint64_t start, std::uint64_t length, bool headersOnly, bool keepAliveAfter)
    {
        // Drop the response if the connection was already force-closed while the handler was still awaiting.
        if (closed)
        {
            return;
        }

        queueOut(std::move(head));
        keepAlive = keepAliveAfter;

        // A HEAD-only response or an empty range has no body to stream after the headers.
        fileMode = !headersOnly && length > 0;
        if (fileMode)
        {
            filePath = std::move(path);
            fileOffset = start;
            fileRemaining = length;
        }

        armWrite();
    }

    void accept(WebSocketOptions settings) override
    {
        if (wsMode || closed)
        {
            return;
        }

        wsOptions = std::move(settings);

        std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Connection: Upgrade\r\n";
        response += "Sec-WebSocket-Accept: " + ReactorHelpers::computeWsAccept(wsKey) + "\r\n";
        if (!wsOptions.protocol.empty())
        {
            response += "Sec-WebSocket-Protocol: " + ReactorHelpers::sanitizeHeader(wsOptions.protocol) + "\r\n";
        }

        response += "\r\n";

        // Drop the consumed handshake request so the frame parser starts on client frame bytes.
        readBuffer.erase(0, wsRequestEnd);

        wsMode = true;
        keepAlive = true;
        queueOut(std::move(response));
        armWrite();
        schedulePing();
    }

    void reject(int statusCode) override
    {
        sendSimpleAndClose(statusCode);
    }

    // Queues one message and calls `done` once it left, refuses alone a message the queue could never hold, and closes a peer whose backlog would pass the limit, since a peer that stops reading must not grow the server without bound.
    void send(std::shared_ptr<const std::string> message, bool binary, SendDone done) override
    {
        const std::size_t size = message->size();
        if (!wsMode || closed || wsCloseAfterFlush || size > wsOptions.maxQueuedBytes)
        {
            if (done)
            {
                done(false);
            }

            return;
        }

        if (wsBuffered + size > wsOptions.maxQueuedBytes)
        {
            wsCloseCode = kWsCloseAbnormal;
            wsCloseReason = kWsStalled;
            closeNow();
            if (done)
            {
                done(false);
            }

            return;
        }

        // A small message travels in the buffer of its head, and a large one keeps the shared buffer it came in, so a broadcast never copies it per peer.
        std::string head = ReactorHelpers::wsHeader(binary ? kWsBinary : kWsText, size);
        if (size <= kInlineChunkBytes)
        {
            head += *message;
            queueOut(std::move(head));
        }
        else
        {
            queueOut(std::move(head));
            queueShared(std::move(message));
        }

        HttpResponse::WriteDone settled;
        if (done)
        {
            // clang-format off
            settled = [done = std::move(done)](std::string error)
            {
                done(error.empty());
            };
            // clang-format on
        }

        wsBuffered += size;
        flushWaiters.push_back({outQueued, size, std::move(settled)});
        wsArmWrite();
    }

    void ping(std::string payload) override
    {
        if (wsMode && !closed && !wsCloseAfterFlush)
        {
            wsSendControl(kWsPing, payload);
        }
    }

    // Sends the close frame with its code and reason and closes once it left, so the close callback hears the code the server chose.
    void close(int code, std::string reason) override
    {
        if (!wsMode || closed || wsCloseAfterFlush)
        {
            return;
        }

        wsSendControl(kWsClose, ReactorHelpers::wsClosePayload(code, reason));
        wsCloseCode = code;
        wsCloseReason = std::move(reason);
        wsCloseAfterFlush = true;
    }

    std::size_t bufferedAmount() const override
    {
        return wsBuffered;
    }

    void onMessage(std::function<void(const std::string&, bool)> handler) override
    {
        wsMessageHandler = std::move(handler);
    }

    void onPong(std::function<void(const std::string&)> handler) override
    {
        wsPongHandler = std::move(handler);
    }

    void onClose(std::function<void(int, const std::string&)> handler) override
    {
        wsCloseHandler = std::move(handler);
    }

private:
    enum class Progress
    {
        NeedMore,
        Detached
    };
    enum class ChunkResult
    {
        Done,
        NeedMore,
        TooLarge,
        Error
    };

    // An I/O attempt either wants the same direction again, wants the opposite direction (TLS renegotiation or handshake), or is done with the connection.
    enum class Io
    {
        Again,
        Switch,
        Detached
    };

    // A handler that runs once the connection closed arms nothing, since the socket it would watch is gone.
    void armRead()
    {
        if (closed)
        {
            return;
        }

        auto self = shared_from_this();
        loop.watchRead(socket, [self]() -> bool
                       { return self->onReadable(); });
    }

    void armWrite()
    {
        if (closed)
        {
            return;
        }

        lastWriteMs = ReactorHelpers::nowMs();
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->onWritable(); });
    }

    void closeNow()
    {
        if (closed)
        {
            return;
        }

        closed = true;
        closeFileFd();
        cancelTimer(pingTimer);
        cancelTimer(drainTimer);
        failFlushes(kClientGone);

        // A closed connection calls no callback again, so it lets go of every one of them, which also frees whatever an owner of the connection captured in them.
        wsMessageHandler = nullptr;
        wsPongHandler = nullptr;
        if (wsCloseHandler)
        {
            auto handler = std::move(wsCloseHandler);
            wsCloseHandler = nullptr;
            handler(wsCloseCode, wsCloseReason);
        }

        if (loop.isRunning())
        {
            loop.closeSocket(socket);
        }
        else
        {
            try
            {
                socket.impl()->close();
            }
            catch (...)
            {
            }
        }

        server->connectionClosed(loop);
    }

    void cancelTimer(EventLoop::TimerId& timer)
    {
        if (timer != 0)
        {
            loop.cancelTimer(timer);
            timer = 0;
        }
    }

    bool onReadable()
    {
        const Io result = tryRead();
        if (result == Io::Again)
        {
            return false;
        }

        if (result == Io::Switch)
        {
            // The TLS layer needs the socket writable before this read can progress.
            auto self = shared_from_this();
            // clang-format off
            loop.watchWrite(socket, [self]() -> bool
            {
                const Io retry = self->tryRead();
                if (retry == Io::Switch)
                {
                    return false;
                }

                if (retry == Io::Again)
                {
                    self->armRead();
                }

                return true;
            });
            // clang-format on
        }

        return true;
    }

    // Reads until the socket would block, up to a bound per readiness so one busy connection cannot hold the loop.
    Io tryRead()
    {
        if (closed)
        {
            return Io::Detached;
        }

        char buffer[kReadChunk];
        for (std::size_t total = 0; total < kReadPassBytes;)
        {
            int received;
            try
            {
                received = socket.receiveBytes(buffer, sizeof(buffer));
            }
            catch (...)
            {
                closeNow();
                return Io::Detached;
            }

            if (received == kSslWantWrite)
            {
                return Io::Switch;
            }

            if (received < 0)
            {
                return Io::Again;
            }

            if (received == 0)
            {
                closeNow();
                return Io::Detached;
            }

            total += static_cast<std::size_t>(received);
            if (draining)
            {
                const std::size_t consumed = std::min(static_cast<std::size_t>(received), drainRemaining);
                drainRemaining -= consumed;
                if (drainRemaining > 0)
                {
                    continue;
                }

                // The drain ends here, and a rejection still on its way closes the connection once it left.
                draining = false;
                cancelTimer(drainTimer);
                if (outQueue.empty())
                {
                    closeNow();
                }

                return Io::Detached;
            }

            // A streaming response only watches the read side to learn the client left, ignoring any bytes it sends mid-stream.
            if (streamingActive)
            {
                continue;
            }

            readBuffer.append(buffer, static_cast<std::size_t>(received));
            if (process() == Progress::Detached)
            {
                return Io::Detached;
            }
        }

        return Io::Again;
    }

    bool onWritable()
    {
        const Io result = tryWrite();
        if (result == Io::Again)
        {
            return false;
        }

        if (result == Io::Switch)
        {
            // The TLS layer needs the socket readable before this write can progress.
            auto self = shared_from_this();
            // clang-format off
            loop.watchRead(socket, [self]() -> bool
            {
                const Io retry = self->tryWrite();
                if (retry == Io::Switch)
                {
                    return false;
                }

                if (retry == Io::Again)
                {
                    self->armWrite();
                }

                return true;
            });
            // clang-format on
        }

        return true;
    }

    Io tryWrite()
    {
        if (closed)
        {
            return Io::Detached;
        }

        while (!outQueue.empty())
        {
            int wrote;
            try
            {
                wrote = sendQueued();
            }
            catch (...)
            {
                closeNow();
                return Io::Detached;
            }

            if (wrote == kSslWantRead)
            {
                return Io::Switch;
            }

            if (wrote < 0)
            {
                settleFlushed();
                return Io::Again;
            }

            if (wrote == 0)
            {
                closeNow();
                return Io::Detached;
            }

            consumeSent(static_cast<std::size_t>(wrote));
            lastWriteMs = ReactorHelpers::nowMs();
        }

        settleFlushed();

        // A file response sends its body before the connection's keep-alive fate is decided.
        if (fileMode && fileRemaining > 0)
        {
#if defined(VARN_HAS_SENDFILE)
            // TLS must encrypt in user space, so only a plaintext socket can hand the file straight to the kernel.
            if (options.tls)
            {
                readNextChunk();
                return Io::Detached;
            }

            if (fileFd < 0)
            {
                fileFd = ::open(filePath.c_str(), O_RDONLY | O_CLOEXEC);

                if (fileFd < 0)
                {
                    closeNow();
                    return Io::Detached;
                }
            }

            while (fileRemaining > 0)
            {
                const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(fileRemaining, kFileChunkBytes));
                std::size_t sent = 0;
                const FileSend result = ReactorHelpers::sendFileToSocket(socket.impl()->sockfd(), fileFd, fileOffset, count, sent);

                if (sent > 0)
                {
                    fileOffset += sent;
                    fileRemaining -= sent;
                    lastWriteMs = ReactorHelpers::nowMs();
                }

                if (result == FileSend::Progress)
                {
                    continue;
                }

                if (result == FileSend::WouldBlock)
                {
                    return Io::Again;
                }

                // An early EOF or a transfer error ends the body, so the connection cannot be reused.
                keepAlive = false;
                break;
            }
#else
            readNextChunk();
            return Io::Detached;
#endif
        }

        fileMode = false;
        closeFileFd();

        // The rejection response is flushed, but the connection stays open until the read side has drained the upload.
        if (draining)
        {
            return Io::Detached;
        }

        // Once the WebSocket handshake response is flushed the connection switches to reading client frames.
        if (wsMode)
        {
            armRead();

            // Drain any frames the client pipelined with the handshake instead of waiting for the next readable event.
            if (!readBuffer.empty())
            {
                wsProcess();
            }

            return Io::Detached;
        }

        // A streaming response has flushed its writes and waits for the next one while watching the read side for a client disconnect.
        if (streamingActive)
        {
            streamWriteArmed = false;

            // Arm the disconnect watcher once since the reader re-pushes itself for the whole stream.
            if (!streamReadArmed)
            {
                streamReadArmed = true;
                armRead();
            }

            return Io::Detached;
        }

        if (!keepAlive || server->stopping.load(std::memory_order_acquire))
        {
            closeNow();
            return Io::Detached;
        }

        // A kept-alive connection preserves any pipelined bytes and either parses the next request now or waits for more.
        resetForNextRequest();
        if (readBuffer.empty())
        {
            armRead();
        }
        else if (process() == Progress::NeedMore)
        {
            armRead();
        }

        return Io::Detached;
    }

    Progress process()
    {
        if (wsMode)
        {
            return wsProcess();
        }

        if (headersEnd == std::string::npos)
        {
            headersEnd = readBuffer.find("\r\n\r\n");
            if (headersEnd == std::string::npos)
            {
                if (readBuffer.size() > kMaxHeaderBytes)
                {
                    sendSimpleAndClose(400);
                    return Progress::Detached;
                }

                return Progress::NeedMore;
            }

            if (!parseHeaders())
            {
                return Progress::Detached;
            }
        }

        if (chunked)
        {
            const ChunkResult decoded = decodeChunks();
            if (decoded == ChunkResult::NeedMore)
            {
                return Progress::NeedMore;
            }

            if (decoded == ChunkResult::TooLarge)
            {
                beginRejectDrain(413, kRejectDrainBytes);
                return Progress::Detached;
            }

            if (decoded == ChunkResult::Error)
            {
                sendSimpleAndClose(400);
                return Progress::Detached;
            }

            requestBody = std::move(chunkedBody);
        }
        else
        {
            // Reject a `Content-Length` that would overflow the offset math regardless of whether the body cap is enabled.
            if (contentLength > readBuffer.max_size() - bodyStart)
            {
                sendSimpleAndClose(400);
                return Progress::Detached;
            }

            if (readBuffer.size() < bodyStart + contentLength)
            {
                return Progress::NeedMore;
            }

            requestBody = readBuffer.substr(bodyStart, contentLength);
            requestEnd = bodyStart + contentLength;
        }

        dispatch();
        return Progress::Detached;
    }

    ChunkResult decodeChunks()
    {
        for (;;)
        {
            const std::size_t lineEnd = readBuffer.find("\r\n", chunkPos);
            if (lineEnd == std::string::npos)
            {
                // An unterminated chunk-size line past the cap is a framing attack rather than a slow client.
                return readBuffer.size() - chunkPos > kMaxChunkLineBytes ? ChunkResult::Error : ChunkResult::NeedMore;
            }

            std::string sizeField = readBuffer.substr(chunkPos, lineEnd - chunkPos);
            const std::size_t extension = sizeField.find(';');
            if (extension != std::string::npos)
            {
                sizeField.erase(extension);
            }

            std::size_t chunkSize = 0;
            if (!parseChunkSize(sizeField, chunkSize))
            {
                return ChunkResult::Error;
            }

            const std::size_t dataStart = lineEnd + 2;
            if (chunkSize == 0)
            {
                return consumeTrailers(dataStart);
            }

            // Reject a declared size that overflows the offset math or exceeds the body cap before the chunk is buffered.
            if (chunkSize > readBuffer.max_size() - dataStart - 2)
            {
                return ChunkResult::Error;
            }

            if (chunkSize > static_cast<std::size_t>(options.maxRequestBodyBytes) - chunkedBody.size())
            {
                return ChunkResult::TooLarge;
            }

            // A chunk needs its declared bytes plus the trailing CRLF before it can be consumed.
            if (readBuffer.size() < dataStart + chunkSize + 2)
            {
                return ChunkResult::NeedMore;
            }

            chunkedBody.append(readBuffer, dataStart, chunkSize);
            chunkPos = dataStart + chunkSize + 2;
        }
    }

    ChunkResult consumeTrailers(std::size_t position)
    {
        const std::size_t trailerStart = position;
        for (;;)
        {
            const std::size_t crlf = readBuffer.find("\r\n", position);
            if (crlf == std::string::npos)
            {
                return readBuffer.size() - position > kMaxChunkLineBytes ? ChunkResult::Error : ChunkResult::NeedMore;
            }

            if (crlf == position)
            {
                requestEnd = crlf + 2;
                return ChunkResult::Done;
            }

            position = crlf + 2;

            // Bound the whole trailer section so a flood of tiny trailer lines cannot grow the buffer unbounded or force a quadratic rescan.
            if (position - trailerStart > kMaxChunkLineBytes)
            {
                return ChunkResult::Error;
            }
        }
    }

    static bool parseChunkSize(const std::string& field, std::size_t& out)
    {
        const std::size_t begin = field.find_first_not_of(" \t");
        if (begin == std::string::npos)
        {
            return false;
        }

        const std::size_t end = field.find_last_not_of(" \t");
        const std::string digits = field.substr(begin, end - begin + 1);
        if (digits.empty() || digits.size() > 16)
        {
            return false;
        }

        out = 0;
        for (char c : digits)
        {
            out <<= 4;
            if (c >= '0' && c <= '9')
            {
                out |= static_cast<std::size_t>(c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                out |= static_cast<std::size_t>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F')
            {
                out |= static_cast<std::size_t>(c - 'A' + 10);
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    static int onUrl(llhttp_t* parser, const char* at, std::size_t length)
    {
        static_cast<HttpConnection*>(parser->data)->pendingTarget.append(at, length);
        return 0;
    }

    static int onHeaderField(llhttp_t* parser, const char* at, std::size_t length)
    {
        static_cast<HttpConnection*>(parser->data)->parseField.append(at, length);
        return 0;
    }

    static int onHeaderValue(llhttp_t* parser, const char* at, std::size_t length)
    {
        static_cast<HttpConnection*>(parser->data)->parseValue.append(at, length);
        return 0;
    }

    static int onHeaderValueComplete(llhttp_t* parser)
    {
        auto* self = static_cast<HttpConnection*>(parser->data);
        self->pendingHeaders.emplace_back(self->parseField, self->parseValue);
        self->parseField.clear();
        self->parseValue.clear();
        return 0;
    }

    static const llhttp_settings_t* requestSettings()
    {
        // clang-format off
        static const llhttp_settings_t settings = []
        {
            llhttp_settings_t table;
            llhttp_settings_init(&table);
            table.on_url = &HttpConnection::onUrl;
            table.on_header_field = &HttpConnection::onHeaderField;
            table.on_header_value = &HttpConnection::onHeaderValue;
            table.on_header_value_complete = &HttpConnection::onHeaderValueComplete;
            return table;
        }();
        // clang-format on
        return &settings;
    }

    static void parseCookies(const std::string& header, std::vector<std::pair<std::string, std::string>>& out)
    {
        std::size_t pos = 0;
        while (pos < header.size())
        {
            const std::size_t semicolon = header.find(';', pos);
            const std::size_t end = semicolon == std::string::npos ? header.size() : semicolon;
            const std::size_t equals = header.find('=', pos);
            if (equals != std::string::npos && equals < end)
            {
                const std::size_t nameBegin = header.find_first_not_of(" \t", pos);
                const std::size_t nameEnd = header.find_last_not_of(" \t", equals - 1);
                if (nameBegin != std::string::npos && nameBegin <= nameEnd)
                {
                    std::string value;
                    const std::size_t valueBegin = header.find_first_not_of(" \t", equals + 1);
                    if (valueBegin != std::string::npos && valueBegin < end)
                    {
                        const std::size_t valueEnd = header.find_last_not_of(" \t", end - 1);
                        value = header.substr(valueBegin, valueEnd - valueBegin + 1);
                    }

                    out.emplace_back(header.substr(nameBegin, nameEnd - nameBegin + 1), value);
                }
            }

            if (semicolon == std::string::npos)
            {
                break;
            }

            pos = semicolon + 1;
        }
    }

    static int hexDigit(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }

        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }

        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }

        return -1;
    }

    static std::string urlDecode(const std::string& in, bool plusAsSpace)
    {
        std::string out;
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size(); ++i)
        {
            const char c = in[i];
            if (c == '%' && i + 2 < in.size())
            {
                const int hi = hexDigit(in[i + 1]);
                const int lo = hexDigit(in[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    out += static_cast<char>((hi << 4) | lo);
                    i += 2;
                    continue;
                }
            }

            out += (plusAsSpace && c == '+') ? ' ' : c;
        }

        return out;
    }

    static void parseQuery(const std::string& query, std::vector<std::pair<std::string, std::string>>& out)
    {
        std::size_t pos = 0;
        while (pos < query.size())
        {
            const std::size_t ampersand = query.find('&', pos);
            const std::size_t end = ampersand == std::string::npos ? query.size() : ampersand;
            const std::size_t equals = query.find('=', pos);
            if (equals != std::string::npos && equals < end)
            {
                std::string name = urlDecode(query.substr(pos, equals - pos), true);
                if (!name.empty())
                {
                    out.emplace_back(name, urlDecode(query.substr(equals + 1, end - equals - 1), true));
                }
            }
            else if (end > pos)
            {
                out.emplace_back(urlDecode(query.substr(pos, end - pos), true), std::string());
            }

            if (ampersand == std::string::npos)
            {
                break;
            }

            pos = ampersand + 1;
        }
    }

    bool parseHeaders()
    {
        pendingMethod.clear();
        pendingTarget.clear();
        pendingHeaders.clear();
        parseField.clear();
        parseValue.clear();

        llhttp_reset(&parser);
        parser.data = this;

        // The `llhttp` parser reads the request line and headers in place and rejects smuggling, duplicate `Content-Length`, and malformed framing.
        const llhttp_errno_t status = llhttp_execute(&parser, readBuffer.data(), headersEnd + 4);
        if (status != HPE_OK && status != HPE_PAUSED_UPGRADE)
        {
            sendSimpleAndClose(400);
            return false;
        }

        const char* methodName = llhttp_method_name(static_cast<llhttp_method_t>(llhttp_get_method(&parser)));
        pendingMethod = methodName != nullptr ? std::string(methodName) : std::string();

        bodyStart = headersEnd + 4;
        chunked = false;
        contentLength = 0;

        const std::string transferEncoding = headerValue(pendingHeaders, "Transfer-Encoding");
        const std::string contentLengthValue = headerValue(pendingHeaders, "Content-Length");
        if (!transferEncoding.empty())
        {
            // Only `chunked` is supported, and it must not be combined with a `Content-Length`.
            if (Poco::icompare(transferEncoding, "chunked") != 0 || !contentLengthValue.empty())
            {
                sendSimpleAndClose(400);
                return false;
            }

            chunked = true;
            chunkPos = bodyStart;
            chunkedBody.clear();
        }
        else if (!contentLengthValue.empty())
        {
            if (contentLengthValue.find_first_not_of("0123456789") != std::string::npos)
            {
                sendSimpleAndClose(400);
                return false;
            }

            try
            {
                contentLength = std::stoull(contentLengthValue);
            }
            catch (...)
            {
                sendSimpleAndClose(413);
                return false;
            }

            // A body the head already declares too large is refused before any of it is read, so a client that sends it whole still meets the answer.
            if (contentLength > static_cast<std::size_t>(options.maxRequestBodyBytes))
            {
                const std::size_t received = readBuffer.size() - bodyStart;
                beginRejectDrain(413, contentLength > received ? contentLength - received : 0);
                return false;
            }
        }

        const std::string connectionHeader = headerValue(pendingHeaders, "Connection");
        if (llhttp_get_http_major(&parser) == 1 && llhttp_get_http_minor(&parser) == 0)
        {
            keepAlive = Poco::icompare(connectionHeader, "keep-alive") == 0;
        }
        else
        {
            keepAlive = Poco::icompare(connectionHeader, "close") != 0;
        }

        // Split the target into path and query in place, decoding only the parts that carry escapes.
        const std::size_t queryStart = pendingTarget.find('?');
        const std::string rawPath = queryStart == std::string::npos ? pendingTarget : pendingTarget.substr(0, queryStart);
        pendingPath = rawPath.find('%') == std::string::npos ? rawPath : urlDecode(rawPath, false);
        if (pendingPath.empty())
        {
            pendingPath = "/";
        }

        pendingQuery.clear();
        if (queryStart == std::string::npos)
        {
            pendingQueryString.clear();
        }
        else
        {
            pendingQueryString = pendingTarget.substr(queryStart + 1);
            parseQuery(pendingQueryString, pendingQuery);
        }

        pendingHost = headerValue(pendingHeaders, "Host");
        pendingCookies.clear();
        parseCookies(headerValue(pendingHeaders, "Cookie"), pendingCookies);

        return true;
    }

    void dispatch()
    {
        // A server that closes takes no new request, even one already on a connection it lets finish.
        if (server->stopping.load(std::memory_order_acquire))
        {
            closeNow();
            return;
        }

        HttpRequest request;
        request.host = std::move(pendingHost);
        request.method = std::move(pendingMethod);
        request.target = std::move(pendingTarget);
        request.path = std::move(pendingPath);
        request.queryString = std::move(pendingQueryString);
        request.headers = std::move(pendingHeaders);
        request.cookies = std::move(pendingCookies);
        request.query = std::move(pendingQuery);
        request.remoteAddress = remoteAddress;
        request.body = std::move(requestBody);

        dispatched = true;

        if (isWebSocketUpgrade(request))
        {
            if (onWebSocket)
            {
                wsKey = headerValue(request.headers, "Sec-WebSocket-Key");
                wsRequestEnd = requestEnd;
                onWebSocket(request, shared_from_this());
            }
            else
            {
                sendSimpleAndClose(404);
            }

            return;
        }

        bool gzipAllowed = false;
#if defined(VARN_HAVE_ZLIB)
        gzipAllowed = options.compress && ReactorHelpers::acceptsGzip(headerValue(request.headers, "Accept-Encoding"));
#endif

        auto response = std::make_shared<ReactorResponse>(shared_from_this(), loop, keepAlive, request.method == "HEAD", gzipAllowed);

        // A matching public file is served ahead of the user handler and streamed off the loop.
        if (staticFiles && staticFiles->tryServe(request, *response))
        {
            return;
        }

        response->armDeadline(options.requestTimeoutMs);
        handler(std::move(request), response);
    }

    static std::string headerValue(const std::vector<std::pair<std::string, std::string>>& headers, const std::string& name)
    {
        for (const auto& [key, value] : headers)
        {
            if (Poco::icompare(key, name) == 0)
            {
                return value;
            }
        }

        return std::string();
    }

    static bool isWebSocketUpgrade(const HttpRequest& request)
    {
        if (Poco::icompare(headerValue(request.headers, "Upgrade"), "websocket") != 0)
        {
            return false;
        }

        std::string connection = headerValue(request.headers, "Connection");
        Poco::toLowerInPlace(connection);
        return connection.find("upgrade") != std::string::npos && !headerValue(request.headers, "Sec-WebSocket-Key").empty();
    }

    Progress wsProcess()
    {
        for (;;)
        {
            WsFrame frame;
            const WsParse result = ReactorHelpers::parseWsFrame(readBuffer, wsOptions.maxMessageBytes, frame);
            if (result == WsParse::NeedMore)
            {
                return Progress::NeedMore;
            }

            if (result == WsParse::TooLarge)
            {
                close(kWsCloseTooLarge, std::string());
                return Progress::Detached;
            }

            if (result == WsParse::Error)
            {
                close(kWsCloseProtocolError, std::string());
                return Progress::Detached;
            }

            readBuffer.erase(0, frame.consumed);
            if (!handleWsFrame(frame))
            {
                return Progress::Detached;
            }
        }
    }

    // Handles one frame and answers whether the connection still reads, closing with the code the protocol names for a frame it refuses.
    bool handleWsFrame(const WsFrame& frame)
    {
        const bool isControl = (frame.opcode & 0x08) != 0;

        // A control frame must be final and at most 125 bytes, and only defined opcodes are accepted.
        if (isControl)
        {
            const bool knownControl = frame.opcode == kWsClose || frame.opcode == kWsPing || frame.opcode == kWsPong;
            if (!knownControl || !frame.fin || frame.payload.size() > 125)
            {
                close(kWsCloseProtocolError, std::string());
                return false;
            }
        }
        else if (frame.opcode != kWsContinuation && frame.opcode != kWsText && frame.opcode != kWsBinary)
        {
            close(kWsCloseProtocolError, std::string());
            return false;
        }

        if (frame.opcode == kWsClose)
        {
            handlePeerClose(frame.payload);
            return false;
        }

        if (frame.opcode == kWsPing)
        {
            wsSendControl(kWsPong, frame.payload);
            return !closed;
        }

        if (frame.opcode == kWsPong)
        {
            wsAwaitingPong = false;
            if (wsPongHandler)
            {
                wsPongHandler(frame.payload);
            }

            return !closed && !wsCloseAfterFlush;
        }

        // Data frames assemble across fragments into one message, enforcing the continuation state machine.
        const bool isContinuation = frame.opcode == kWsContinuation;
        if (isContinuation != wsFragmentOpen)
        {
            // A continuation without an open message, or a fresh data frame while one is still open, violates the framing.
            close(kWsCloseProtocolError, std::string());
            return false;
        }

        if (!isContinuation)
        {
            wsMessage.clear();
            wsMessageBinary = frame.opcode == kWsBinary;
        }

        if (wsMessage.size() + frame.payload.size() > wsOptions.maxMessageBytes)
        {
            close(kWsCloseTooLarge, std::string());
            return false;
        }

        // An unfragmented message is handed over in the buffer it was unmasked into, and only fragments gather.
        if (!isContinuation && frame.fin)
        {
            return deliverWsMessage(frame.payload);
        }

        wsMessage += frame.payload;
        wsFragmentOpen = !frame.fin;
        if (!frame.fin)
        {
            return true;
        }

        const bool reading = deliverWsMessage(wsMessage);
        wsMessage.clear();
        return reading;
    }

    // A text message must be UTF-8, which a browser enforces too, so one that is not closes the connection with the code for invalid data.
    bool deliverWsMessage(const std::string& message)
    {
        if (!wsMessageBinary && !HttpText::validUtf8(message))
        {
            close(kWsCloseInvalidData, std::string());
            return false;
        }

        if (wsMessageHandler)
        {
            wsMessageHandler(message, wsMessageBinary);
        }

        return !closed && !wsCloseAfterFlush;
    }

    // Answers the close of the peer with its code, which the close callback then receives with the reason, and closes once the answer left.
    void handlePeerClose(const std::string& payload)
    {
        if (payload.size() == 1)
        {
            close(kWsCloseProtocolError, std::string());
            return;
        }

        int code = kWsCloseNoStatus;
        std::string reason;
        if (payload.size() >= 2)
        {
            code = (static_cast<unsigned char>(payload[0]) << 8) | static_cast<unsigned char>(payload[1]);
            reason = payload.substr(2);
        }

        if (code != kWsCloseNoStatus && !WebSocketConnection::validCloseCode(code))
        {
            close(kWsCloseProtocolError, std::string());
            return;
        }

        if (!HttpText::validUtf8(reason))
        {
            close(kWsCloseInvalidData, std::string());
            return;
        }

        if (wsCloseAfterFlush)
        {
            return;
        }

        wsCloseCode = code;
        wsCloseReason = std::move(reason);
        wsSendControl(kWsClose, code == kWsCloseNoStatus ? std::string() : payload.substr(0, 2));
        wsCloseAfterFlush = true;
    }

    void wsSendControl(int opcode, const std::string& payload)
    {
        if (closed)
        {
            return;
        }

        std::string frame = ReactorHelpers::wsHeader(opcode, payload.size());
        frame += payload;
        queueOut(std::move(frame));
        wsArmWrite();
    }

    void wsArmWrite()
    {
        if (wsWriting || closed)
        {
            return;
        }

        wsWriting = true;
        lastWriteMs = ReactorHelpers::nowMs();
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->wsOnWritable(); });
    }

    bool wsOnWritable()
    {
        if (closed)
        {
            return true;
        }

        while (!outQueue.empty())
        {
            int wrote;
            try
            {
                wrote = sendQueued();
            }
            catch (...)
            {
                closeNow();
                return true;
            }

            if (wrote < 0)
            {
                settleFlushed();
                return false;
            }

            if (wrote == 0)
            {
                closeNow();
                return true;
            }

            consumeSent(static_cast<std::size_t>(wrote));
            lastWriteMs = ReactorHelpers::nowMs();
        }

        settleFlushed();
        wsWriting = false;
        if (wsCloseAfterFlush)
        {
            closeNow();
        }

        return true;
    }

    // Pings the peer at the interval of its route and closes one that did not answer the previous ping, which finds a peer that vanished without a close.
    void schedulePing()
    {
        if (wsOptions.pingIntervalMs <= 0)
        {
            return;
        }

        std::weak_ptr<HttpConnection> weak = weak_from_this();
        // clang-format off
        pingTimer = loop.postDelayed(wsOptions.pingIntervalMs, [weak]()
        {
            if (auto self = weak.lock())
            {
                self->onPingDue();
            }
        });
        // clang-format on
    }

    void onPingDue()
    {
        pingTimer = 0;
        if (closed || wsCloseAfterFlush)
        {
            return;
        }

        if (wsAwaitingPong)
        {
            wsCloseCode = kWsCloseAbnormal;
            wsCloseReason = kWsNoPong;
            closeNow();
            return;
        }

        wsAwaitingPong = true;
        wsSendControl(kWsPing, std::string());
        schedulePing();
    }

    void queueOut(std::string segment)
    {
        if (segment.empty())
        {
            return;
        }

        outQueued += segment.size();
        outQueue.push_back({std::move(segment), nullptr});
    }

    void queueShared(std::shared_ptr<const std::string> segment)
    {
        if (segment->empty())
        {
            return;
        }

        outQueued += segment->size();
        outQueue.push_back({std::string(), std::move(segment)});
    }

    // Sends as much of the queue as the socket takes in one call, gathering several buffers on a plaintext POSIX socket.
    int sendQueued()
    {
        const OutSegment& front = outQueue.front();

        // TLS encrypts one buffer at a time and Windows reports a full socket of a gathered send as an error, so those send the front buffer alone.
#if !defined(_WIN32)
        if (!options.tls && outQueue.size() > 1)
        {
            Poco::Net::SocketBufVec buffers;
            std::size_t total = 0;
            for (std::size_t i = 0; i < outQueue.size() && i < kMaxGatherSegments && total < static_cast<std::size_t>(INT_MAX); ++i)
            {
                const OutSegment& segment = outQueue[i];
                const std::size_t skip = i == 0 ? outOffset : 0;
                const std::size_t length = std::min(segment.size() - skip, static_cast<std::size_t>(INT_MAX) - total);
                // The gathered send only reads its buffers, while the vector the platform takes is not const.
                buffers.push_back(Poco::Net::Socket::makeBuffer(const_cast<char*>(segment.data()) + skip, length));
                total += length;
            }

            return socket.sendBytes(buffers);
        }
#endif

        const std::size_t remaining = front.size() - outOffset;
        return socket.sendBytes(front.data() + outOffset, static_cast<int>(std::min(remaining, static_cast<std::size_t>(INT_MAX))));
    }

    void consumeSent(std::size_t sent)
    {
        outSent += sent;
        while (sent > 0)
        {
            const std::size_t left = outQueue.front().size() - outOffset;
            if (sent < left)
            {
                outOffset += sent;
                return;
            }

            sent -= left;
            outQueue.pop_front();
            outOffset = 0;
        }
    }

    // Tells every write still waiting for the socket that its data will never leave.
    void failFlushes(const char* reason)
    {
        std::deque<FlushWaiter> abandoned;
        abandoned.swap(flushWaiters);
        wsBuffered = 0;
        for (FlushWaiter& waiter : abandoned)
        {
            if (waiter.done)
            {
                waiter.done(reason);
            }
        }
    }

    // Tells every write whose bytes all left for the socket that they did.
    void settleFlushed()
    {
        while (!flushWaiters.empty() && flushWaiters.front().mark <= outSent)
        {
            FlushWaiter waiter = std::move(flushWaiters.front());
            flushWaiters.pop_front();
            wsBuffered -= waiter.payload;
            if (waiter.done)
            {
                waiter.done(std::string());
            }
        }
    }

    void closeFileFd()
    {
#if defined(VARN_HAS_SENDFILE)
        if (fileFd >= 0)
        {
            ::close(fileFd);
            fileFd = -1;
        }
#endif
    }

    void readNextChunk()
    {
        const std::uint64_t offset = fileOffset;
        const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(fileRemaining, kFileChunkBytes));
        auto self = shared_from_this();
        // clang-format off
        runtime.ioPool().post([self, path = filePath, offset, want]()
        {
            std::string chunk = readFileRange(path, offset, want);
            self->loop.post([self, chunk = std::move(chunk), want]() mutable { self->onFileChunk(std::move(chunk), want); });
        });
        // clang-format on
    }

    void onFileChunk(std::string chunk, std::size_t want)
    {
        if (closed)
        {
            return;
        }

        const std::size_t got = chunk.size();
        fileOffset += got;

        // A short read means the file ended before its declared length, so the stream stops and the connection closes.
        if (got < want)
        {
            fileRemaining = 0;
            keepAlive = false;
        }
        else
        {
            fileRemaining -= got;
        }

        if (chunk.empty())
        {
            fileMode = false;
            closeNow();
            return;
        }

        queueOut(std::move(chunk));
        armWrite();
    }

    static std::string readFileRange(const std::string& path, std::uint64_t offset, std::size_t want)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return std::string();
        }

        file.seekg(static_cast<std::streamoff>(offset));
        std::string buffer(want, '\0');
        file.read(buffer.data(), static_cast<std::streamsize>(want));
        buffer.resize(static_cast<std::size_t>(file.gcount()));
        return buffer;
    }

    void sendSimpleAndClose(int code)
    {
        std::string body = std::string(ReactorHelpers::reasonPhrase(code)) + ".";
        std::string head = "HTTP/1.1 " + std::to_string(code) + " " + ReactorHelpers::reasonPhrase(code) + "\r\n";
        head += "Content-Type: text/plain; charset=utf-8\r\n";
        head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        head += "Connection: close\r\n\r\n";
        submitResponse(std::move(head), std::move(body), false);
    }

    // Answers at once and closes, after reading off at most a bounded part of the body the client may still be sending, so the close is a clean end rather than a reset that loses the answer, and a large upload never holds the server.
    void beginRejectDrain(int code, std::size_t remaining)
    {
        drainRemaining = std::min(remaining, kRejectDrainBytes);
        readBuffer.clear();

        if (drainRemaining == 0)
        {
            sendSimpleAndClose(code);
            return;
        }

        draining = true;
        sendSimpleAndClose(code);
        armRead();

        std::weak_ptr<HttpConnection> weak = weak_from_this();
        // clang-format off
        drainTimer = loop.postDelayed(kRejectDrainMs, [weak]()
        {
            if (auto self = weak.lock())
            {
                self->drainTimer = 0;
                self->closeNow();
            }
        });
        // clang-format on
    }

    void resetForNextRequest()
    {
        std::string leftover = readBuffer.substr(requestEnd);
        readBuffer = std::move(leftover);
        headersEnd = std::string::npos;
        bodyStart = 0;
        contentLength = 0;
        chunked = false;
        chunkPos = 0;
        requestEnd = 0;
        chunkedBody.clear();
        requestBody.clear();
        outQueue.clear();
        outOffset = 0;
        fileMode = false;
        filePath.clear();
        fileOffset = 0;
        fileRemaining = 0;
        closeFileFd();
        dispatched = false;
        streamingActive = false;
        streamWriteArmed = false;
        streamReadArmed = false;
        requestStartMs = ReactorHelpers::nowMs();
    }

    Poco::Net::StreamSocket socket;
    EventLoop& loop;
    Runtime& runtime;
    HttpServerOptions options;
    HttpHandler handler;
    WebSocketHandler onWebSocket;
    std::shared_ptr<StaticFileHandler> staticFiles;
    std::shared_ptr<ReactorServerState> server;
    std::string remoteAddress;

    std::string readBuffer;
    std::size_t headersEnd = std::string::npos;
    std::size_t bodyStart = 0;
    std::size_t contentLength = 0;
    std::size_t requestEnd = 0;
    bool draining = false;
    std::size_t drainRemaining = 0;
    bool chunked = false;
    std::size_t chunkPos = 0;
    std::string chunkedBody;
    std::string requestBody;
    bool keepAlive = false;
    bool closed = false;
    bool dispatched = false;
    bool streamingActive = false;
    bool streamWriteArmed = false;
    bool streamReadArmed = false;
    long long requestStartMs = 0;
    long long lastWriteMs = 0;

    struct FlushWaiter
    {
        std::uint64_t mark;
        std::size_t payload;
        HttpResponse::WriteDone done;
    };

    // A buffer of the queue either owns its bytes or shares the message a broadcast hands to every peer.
    struct OutSegment
    {
        std::string owned;
        std::shared_ptr<const std::string> shared;

        const char* data() const
        {
            return shared ? shared->data() : owned.data();
        }

        std::size_t size() const
        {
            return shared ? shared->size() : owned.size();
        }
    };

    std::deque<OutSegment> outQueue;
    std::size_t outOffset = 0;
    std::uint64_t outQueued = 0;
    std::uint64_t outSent = 0;
    std::deque<FlushWaiter> flushWaiters;

    bool fileMode = false;
    std::string filePath;
    std::uint64_t fileOffset = 0;
    std::uint64_t fileRemaining = 0;
#if defined(VARN_HAS_SENDFILE)
    int fileFd = -1;
#endif

    bool wsMode = false;
    WebSocketOptions wsOptions;
    std::string wsMessage;
    bool wsMessageBinary = false;
    bool wsFragmentOpen = false;
    std::size_t wsBuffered = 0;
    bool wsWriting = false;
    bool wsCloseAfterFlush = false;
    bool wsAwaitingPong = false;
    int wsCloseCode = kWsCloseAbnormal;
    std::string wsCloseReason;
    std::string wsKey;
    std::size_t wsRequestEnd = 0;
    EventLoop::TimerId pingTimer = 0;
    EventLoop::TimerId drainTimer = 0;
    std::function<void(const std::string&, bool)> wsMessageHandler;
    std::function<void(const std::string&)> wsPongHandler;
    std::function<void(int, const std::string&)> wsCloseHandler;

    std::string pendingMethod;
    std::string pendingTarget;
    std::string pendingPath;
    std::string pendingQueryString;
    std::string pendingHost;
    std::vector<std::pair<std::string, std::string>> pendingHeaders;
    std::vector<std::pair<std::string, std::string>> pendingCookies;
    std::vector<std::pair<std::string, std::string>> pendingQuery;

    llhttp_t parser;
    std::string parseField;
    std::string parseValue;
};

// Builds the head with the framing the body takes, so the length or the chunked encoding the server chose is the only one the client reads.
std::string ReactorResponse::buildHead(Framing framing, std::size_t contentLength)
{
    if (statusCode < 100 || statusCode > 599)
    {
        statusCode = 500;
    }

    std::string head;
    head.reserve(256);
    head += "HTTP/1.1 ";
    head += std::to_string(statusCode);
    head += ' ';
    head += ReactorHelpers::reasonPhrase(statusCode);
    head += "\r\n";

    bool hasContentType = false;
    for (const auto& [name, value] : headerMap)
    {
        if (framing != Framing::Declared && Poco::icompare(name, "content-length") == 0)
        {
            continue;
        }

        head += name;
        head += ": ";
        head += value;
        head += "\r\n";
        if (Poco::icompare(name, "content-type") == 0)
        {
            hasContentType = true;
        }
    }

    for (const auto& [name, value] : extraHeaders)
    {
        head += name;
        head += ": ";
        head += value;
        head += "\r\n";
        if (Poco::icompare(name, "content-type") == 0)
        {
            hasContentType = true;
        }
    }

    if (!hasContentType)
    {
        head += "Content-Type: text/plain; charset=utf-8\r\n";
    }

    if (framing == Framing::Length)
    {
        head += "Content-Length: ";
        head += std::to_string(contentLength);
        head += "\r\n";
    }

    if (framing == Framing::Chunked)
    {
        head += "Transfer-Encoding: chunked\r\n";
    }

    head += keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    head += "\r\n";
    return head;
}

void ReactorResponse::maybeCompress(std::string& body)
{
#if defined(VARN_HAVE_ZLIB)
    if (!gzipAllowed || body.size() < kCompressThreshold)
    {
        return;
    }

    std::string contentType = "text/plain; charset=utf-8";
    bool alreadyEncoded = false;
    for (const auto& [name, value] : headerMap)
    {
        if (Poco::icompare(name, "content-type") == 0)
        {
            contentType = value;
        }
        else if (Poco::icompare(name, "content-encoding") == 0)
        {
            alreadyEncoded = true;
        }
    }

    for (const auto& [name, value] : extraHeaders)
    {
        if (Poco::icompare(name, "content-encoding") == 0)
        {
            alreadyEncoded = true;
        }
    }

    if (alreadyEncoded || !ReactorHelpers::compressibleType(contentType))
    {
        return;
    }

    std::string compressed;
    if (!ReactorHelpers::gzipEncode(body, compressed) || compressed.size() >= body.size())
    {
        return;
    }

    body = std::move(compressed);
    headerMap["Content-Encoding"] = "gzip";
    extraHeaders.emplace_back("Vary", "Accept-Encoding");
#else
    (void)body;
#endif
}

// Opens the stream with its head: the length the handler declared through "Content-Length" frames the body when it set a valid one, and chunked encoding frames it otherwise.
std::vector<std::string> ReactorResponse::startStream()
{
    streaming = true;
    cancelDeadline();

    // A bodyless status carries no framing and no body, and a HEAD request advertises the framing without the body.
    if (statusCode == 204 || statusCode == 304)
    {
        discardBody = true;
        return {buildHead(Framing::None, 0)};
    }

    discardBody = headersOnly;

    std::string declared;
    for (const auto& [name, value] : headerMap)
    {
        if (Poco::icompare(name, "content-length") == 0)
        {
            declared = value;
        }
    }

    // Nineteen digits always fit the unsigned width the count is kept in.
    if (!declared.empty() && declared.size() <= 19 && declared.find_first_not_of("0123456789") == std::string::npos)
    {
        declaredRemaining = std::stoull(declared);
        return {buildHead(Framing::Declared, 0)};
    }

    chunkedMode = true;
    return {buildHead(Framing::Chunked, 0)};
}

// Adds the buffers that carry one write to the stream, and answers false when it would pass the length the handler declared.
bool ReactorResponse::frame(std::string chunk, std::vector<std::string>& segments)
{
    if (discardBody || chunk.empty())
    {
        return true;
    }

    if (!chunkedMode)
    {
        if (chunk.size() > declaredRemaining)
        {
            return false;
        }

        declaredRemaining -= chunk.size();
        segments.push_back(std::move(chunk));
        return true;
    }

    char digits[17];
    const auto sized = std::to_chars(digits, digits + sizeof(digits), chunk.size(), 16);
    std::string sizeLine(digits, sized.ptr);
    sizeLine += "\r\n";

    // A small chunk travels as one buffer with its framing, and a large one keeps its own buffer between its size line and its line end.
    if (chunk.size() <= kInlineChunkBytes)
    {
        sizeLine += chunk;
        sizeLine += "\r\n";
        segments.push_back(std::move(sizeLine));
        return true;
    }

    segments.push_back(std::move(sizeLine));
    segments.push_back(std::move(chunk));
    segments.emplace_back("\r\n");
    return true;
}

void ReactorResponse::write(std::string chunk, WriteDone done)
{
    if (finished)
    {
        if (done)
        {
            done(kResponseEnded);
        }

        return;
    }

    std::vector<std::string> segments;
    if (!streaming)
    {
        segments = startStream();
    }

    // A write past the declared length is refused whole, while the head it opened still goes out.
    const bool fits = frame(std::move(chunk), segments);
    if (!fits)
    {
        if (!segments.empty())
        {
            connection->submitStream(std::move(segments), nullptr, keepAlive, false);
        }

        if (done)
        {
            done(kPastContentLength);
        }

        return;
    }

    connection->submitStream(std::move(segments), std::move(done), keepAlive, false);
}

void ReactorResponse::end(std::string body)
{
    if (finished)
    {
        return;
    }

    finished = true;
    cancelDeadline();

    // A streamed response sends the body as its last write and closes the chunked encoding.
    if (streaming)
    {
        std::vector<std::string> segments;
        if (!frame(std::move(body), segments))
        {
            log::Log::error("HttpServer", kPastContentLength);
        }

        if (chunkedMode && !discardBody)
        {
            segments.emplace_back("0\r\n\r\n");
        }

        // A body shorter than its declared length leaves the client waiting for the rest, so the connection closes once the stream is sent.
        if (!chunkedMode && !discardBody && declaredRemaining > 0)
        {
            keepAlive = false;
        }

        connection->submitStream(std::move(segments), nullptr, keepAlive, true);
        return;
    }

    const bool bodyless = statusCode == 204 || statusCode == 304;
    if (!bodyless)
    {
        maybeCompress(body);
    }

    std::string head = buildHead(bodyless ? Framing::None : Framing::Length, body.size());

    // A `HEAD` request still advertises `Content-Length` but carries no body.
    if (bodyless || headersOnly)
    {
        body = std::string();
    }

    connection->submitResponse(std::move(head), std::move(body), keepAlive);
}

// Ends the response of a handler that failed: one still unsent answers 500, and a stream sends what it holds and closes without its end, so the client sees it cut short.
void ReactorResponse::fail()
{
    if (finished)
    {
        return;
    }

    if (!streaming)
    {
        statusCode = 500;
        headerMap["Content-Type"] = "text/plain; charset=utf-8";
        end("Internal server error.");
        return;
    }

    finished = true;
    cancelDeadline();
    keepAlive = false;
    connection->submitStream({}, nullptr, keepAlive, true);
}

void ReactorResponse::sendFile(const std::string& path, std::uint64_t start, std::uint64_t length, bool headersOnly)
{
    if (finished)
    {
        return;
    }

    finished = true;
    cancelDeadline();

    const bool bodyless = statusCode == 204 || statusCode == 304;
    std::string head = buildHead(bodyless ? Framing::None : Framing::Length, static_cast<std::size_t>(length));
    connection->streamFile(std::move(head), path, start, length, headersOnly, keepAlive);
}

// Answers 504 when the handler has not started its answer by the deadline, and leaves the handler to run on with a response that refuses whatever it sends later.
void ReactorResponse::armDeadline(long long timeoutMs)
{
    if (timeoutMs <= 0)
    {
        return;
    }

    std::weak_ptr<ReactorResponse> weak = weak_from_this();
    // clang-format off
    deadline = loop.postDelayed(timeoutMs, [weak]()
    {
        if (auto self = weak.lock())
        {
            self->deadline = 0;
            self->expire();
        }
    });
    // clang-format on
}

void ReactorResponse::expire()
{
    if (finished || streaming)
    {
        return;
    }

    statusCode = 504;
    headerMap.clear();
    extraHeaders.clear();
    keepAlive = false;
    end(std::string(ReactorHelpers::reasonPhrase(504)) + ".");
}

void ReactorResponse::cancelDeadline()
{
    if (deadline != 0)
    {
        loop.cancelTimer(deadline);
        deadline = 0;
    }
}

void ReactorHelpers::scheduleSweep(EventLoop& loop, std::shared_ptr<ReactorServerState> state, long long timeoutMs)
{
    // clang-format off
    state->sweepTimer = loop.postDelayed(kSweepIntervalMs, [&loop, state, timeoutMs]()
    {
        state->sweepTimer = 0;
        if (state->forced.load(std::memory_order_acquire))
        {
            // Force-close every live connection so its close handler runs and releases its Lua reference, instead of leaking on shutdown.
            for (const auto& connection : state->liveConnections())
            {
                connection->forceClose();
            }

            state->registry.clear();
            return;
        }

        // Close connections still idle-receiving past the deadline and compact the registry in place.
        const long long now = ReactorHelpers::nowMs();
        auto& connections = state->registry;
        std::size_t kept = 0;
        for (std::size_t i = 0; i < connections.size(); ++i)
        {
            auto connection = connections[i].lock();
            if (!connection || connection->isClosed())
            {
                continue;
            }

            // A non-positive timeout keeps connections open, so the sweep only compacts the registry then.
            if (timeoutMs > 0 && connection->isStale(now, timeoutMs))
            {
                connection->forceClose();
                continue;
            }

            connections[kept++] = connections[i];
        }

        connections.resize(kept);

        // A server that closes stops sweeping once its last connection is gone.
        if (state->closing && connections.empty())
        {
            return;
        }

        ReactorHelpers::scheduleSweep(loop, state, timeoutMs);
    });
    // clang-format on
}

} // namespace

void ReactorServerState::connectionClosed(EventLoop& loop)
{
    --open;
    settleIfDrained(loop);
}

// Hands the close its answer once the listener is closed and no connection is left, through the loop, since what answers it may release the server.
void ReactorServerState::settleIfDrained(EventLoop& loop)
{
    if (!closing || open > 0 || closeWaiters.empty())
    {
        return;
    }

    if (closeDeadline != 0)
    {
        loop.cancelTimer(closeDeadline);
        closeDeadline = 0;
    }

    if (sweepTimer != 0)
    {
        loop.cancelTimer(sweepTimer);
        sweepTimer = 0;
    }

    registry.clear();
    std::vector<std::function<void()>> waiters;
    waiters.swap(closeWaiters);

    // clang-format off
    loop.post([waiters = std::move(waiters)]()
    {
        for (const auto& waiter : waiters)
        {
            waiter();
        }
    });
    // clang-format on
}

// Locks every connection still open into a list, so closing one, which runs its close callback, never changes what is being walked.
std::vector<std::shared_ptr<HttpConnection>> ReactorServerState::liveConnections() const
{
    std::vector<std::shared_ptr<HttpConnection>> live;
    live.reserve(registry.size());
    for (const auto& weak : registry)
    {
        if (auto connection = weak.lock())
        {
            live.push_back(std::move(connection));
        }
    }

    return live;
}

ReactorHttpServer::ReactorHttpServer(Runtime& rt, HttpServerOptions opts, HttpHandler onRequest, WebSocketHandler onWebSocket)
    : runtime(rt)
    , serverOptions(std::move(opts))
    , httpHandler(std::move(onRequest))
    , webSocketHandler(std::move(onWebSocket))
    , state(std::make_shared<ReactorServerState>())
{
}

ReactorHttpServer::~ReactorHttpServer()
{
    stop();
}

// Binds without sharing the port unless the server asks to, so a second server on a busy address fails instead of splitting the connections of the first.
// POSIX keeps `SO_REUSEADDR` so a restarted server rebinds a port in `TIME_WAIT`, while on Windows that flag would let another socket take the port, so the socket there binds with `SO_EXCLUSIVEADDRUSE`, which Poco sets whenever the reuse of the address is off.
void ReactorHttpServer::bindListener(const Poco::Net::SocketAddress& address, int backlog)
{
#if defined(_WIN32)
    const bool reuseAddress = serverOptions.reusePort;
#else
    const bool reuseAddress = true;
#endif
    const bool reusePort = serverOptions.reusePort;

#ifdef VARN_ENABLE_TLS
    if (serverOptions.tls)
    {
        Poco::Net::Context::Ptr context = TlsServerContext::create(serverOptions);
        TlsServerContext::initializeSslManager(context);
        Poco::Net::SecureServerSocket secure(context);
        secure.bind(address, reuseAddress, reusePort);
        secure.listen(backlog);
        listener = secure;
        return;
    }
#endif

    Poco::Net::ServerSocket plain;
    plain.bind(address, reuseAddress, reusePort);
    plain.listen(backlog);
    listener = plain;
}

// Starts listening, and fails with a message that names the address and the cause, the one for a port another socket holds included.
void ReactorHttpServer::start()
{
    if (started.load(std::memory_order_acquire))
    {
        return;
    }

    const std::string endpoint = serverOptions.host + ":" + std::to_string(serverOptions.port);
    const int backlog = std::clamp(serverOptions.maxQueued, 1, 65535);
    try
    {
        bindListener(Poco::Net::SocketAddress(serverOptions.host, static_cast<Poco::UInt16>(serverOptions.port)), backlog);
    }
    catch (const Poco::Net::NetException& ex)
    {
        if (ex.code() == POCO_EADDRINUSE)
        {
            throw std::runtime_error("[HttpServer] The address " + endpoint + " is already in use.");
        }

        throw std::runtime_error("[HttpServer] The server could not listen on " + endpoint + ". " + ex.displayText());
    }
    catch (const Poco::Exception& ex)
    {
        throw std::runtime_error("[HttpServer] The server could not listen on " + endpoint + ". " + ex.displayText());
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error("[HttpServer] The server could not listen on " + endpoint + ". " + ex.what());
    }

    started.store(true, std::memory_order_release);
    boundPort = listener.address().port();
    listener.setBlocking(false);

#if defined(__linux__)
    // Defer accept until the client's first request bytes arrive, so the first read always has data and connect-only floods never reach the loop.
    {
        const int deferSeconds = std::clamp(serverOptions.keepAliveTimeoutSeconds, 1, 600);
        ::setsockopt(listener.impl()->sockfd(), IPPROTO_TCP, TCP_DEFER_ACCEPT, &deferSeconds, sizeof(deferSeconds));
    }
#endif

    const long long timeoutMs = static_cast<long long>(serverOptions.keepAliveTimeoutSeconds) * 1000;

    std::shared_ptr<StaticFileHandler> staticFiles;
    if (serverOptions.servePublic)
    {
        staticFiles = std::make_shared<StaticFileHandler>(serverOptions.publicDir, serverOptions.directoryListing);
    }

    EventLoop& loop = runtime.mainLoop();
    Runtime& rt = runtime;
    HttpServerOptions opts = serverOptions;
    HttpHandler onRequest = httpHandler;
    WebSocketHandler onWebSocket = webSocketHandler;
    std::shared_ptr<ReactorServerState> shared = state;
    Poco::Net::ServerSocket server = listener;

    // clang-format off
    loop.watchRead(listener, [&loop, &rt, opts, onRequest, onWebSocket, staticFiles, shared, server]() mutable -> bool
    {
        if (shared->stopping.load(std::memory_order_acquire))
        {
            return true;
        }

        // Drain every pending connection so a single readiness event accepts the full backlog.
        for (;;)
        {
            Poco::Net::StreamSocket accepted;
            try
            {
                accepted = server.acceptConnection();
            }
            catch (...)
            {
                break;
            }

            auto connection = std::make_shared<HttpConnection>(std::move(accepted), loop, rt, opts, onRequest, onWebSocket, staticFiles, shared);
            shared->registry.push_back(connection);
            ++shared->open;
            connection->start();
        }

        return false;
    });
    // clang-format on

    // The sweep also compacts the connection registry, so it runs even when the idle and slowloris timeout is disabled.
    ReactorHelpers::scheduleSweep(loop, state, timeoutMs);
}

void ReactorHttpServer::stop()
{
    state->forced.store(true, std::memory_order_release);
    if (!started.exchange(false, std::memory_order_acq_rel))
    {
        return;
    }

    state->stopping.store(true, std::memory_order_release);
    EventLoop& loop = runtime.mainLoop();
    if (loop.isRunning())
    {
        loop.closeSocket(listener);
        return;
    }

    try
    {
        listener.impl()->close();
    }
    catch (...)
    {
    }
}

// Stops listening and ends the connections, at once or once the requests in flight finished and at the latest at the deadline when the close is graceful, and calls `done` through the loop once the last connection is gone.
void ReactorHttpServer::close(bool graceful, long long timeoutMs, std::function<void()> done)
{
    EventLoop& loop = runtime.mainLoop();
    state->closeWaiters.push_back(std::move(done));
    state->closing = true;

    if (started.exchange(false, std::memory_order_acq_rel))
    {
        state->stopping.store(true, std::memory_order_release);
        loop.closeSocket(listener);
    }

    // A later close that is not graceful ends what an earlier graceful one still waits for.
    if (graceful && state->closeDeadline == 0)
    {
        std::weak_ptr<ReactorServerState> weak = state;
        // clang-format off
        state->closeDeadline = loop.postDelayed(timeoutMs, [weak, &loop]()
        {
            auto shared = weak.lock();
            if (!shared)
            {
                return;
            }

            shared->closeDeadline = 0;
            for (const auto& connection : shared->liveConnections())
            {
                connection->shutdown(false);
            }

            shared->settleIfDrained(loop);
        });
        // clang-format on
    }

    for (const auto& connection : state->liveConnections())
    {
        connection->shutdown(graceful);
    }

    state->settleIfDrained(loop);
}

const std::string& ReactorHttpServer::host() const
{
    return serverOptions.host;
}

int ReactorHttpServer::port() const
{
    return boundPort;
}

bool ReactorHttpServer::tls() const
{
    return serverOptions.tls;
}

} // namespace varn::http
