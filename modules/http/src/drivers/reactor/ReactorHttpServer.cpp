#include "varn/http/drivers/reactor/ReactorHttpServer.h"

#include "varn/http/StaticFileHandler.h"
#include "varn/log/Log.h"
#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"

#include <Poco/Base64Encoder.h>
#include <Poco/Net/SocketAddress.h>
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
#include <chrono>
#include <climits>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
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
constexpr std::size_t kFileChunkBytes = 64 * 1024;
constexpr long long kSweepIntervalMs = 1000;

// these mirror Poco::Net::SecureStreamSocket::ERR_SSL_WANT_READ and ERR_SSL_WANT_WRITE, so tls i/o needs no ssl header here
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
constexpr std::size_t kWsMaxMessageBytes = 16 * 1024 * 1024;
constexpr std::size_t kWsMaxOutBytes = 16 * 1024 * 1024;
constexpr std::size_t kStreamMaxOutBytes = 16 * 1024 * 1024;

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
    Error
};

class HttpConnection;

class ReactorHelpers
{
public:
#if defined(VARN_HAS_SENDFILE)
    static FileSend sendFileToSocket(int socketFd, int fileFd, std::uint64_t offset, std::size_t count, std::size_t& sent)
    {
        // hand up to count bytes from the open file straight to the socket without copying through user space, reporting the bytes moved in sent
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

    // drops the already-sent prefix once it dominates the buffer, so a peer that only ever drains partially cannot grow it with the connection's cumulative traffic
    static void compactSent(std::string& buffer, std::size_t& offset)
    {
        if (offset == 0 || offset < buffer.size() / 2)
        {
            return;
        }

        buffer.erase(0, offset);
        offset = 0;
    }

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
            // leave the reason phrase empty for an unlisted code since http allows it and inventing "OK" would misdescribe the status
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
        // gzip a body with the standard 16+MAX_WBITS window so the output carries a gzip header rather than raw deflate
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
        // json, xml and any text/* payload compress well, whereas already-encoded media gains nothing
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

    static WsParse parseWsFrame(const std::string& buffer, WsFrame& frame)
    {
        const std::size_t size = buffer.size();
        if (size < 2)
        {
            return WsParse::NeedMore;
        }

        const auto byteAt = [&](std::size_t i)
        { return static_cast<unsigned char>(buffer[i]); };
        frame.fin = (byteAt(0) & 0x80) != 0;

        // no extension is negotiated, so any reserved bit set is a protocol error
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

        if (!masked || length > kWsMaxMessageBytes)
        {
            return WsParse::Error;
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

    static std::string buildWsFrame(int opcode, const std::string& payload)
    {
        std::string out;
        out.push_back(static_cast<char>(0x80 | (opcode & 0x0F)));

        const std::size_t length = payload.size();
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

        out += payload;
        return out;
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

    static void scheduleSweep(EventLoop& loop, std::shared_ptr<std::vector<std::weak_ptr<HttpConnection>>> registry, std::shared_ptr<std::atomic<bool>> stopping, long long timeoutMs);
};

// the response is filled by the lua handler on the loop thread and handed to the connection's i/o thread when end runs
class ReactorResponse final : public HttpResponse
{
public:
    ReactorResponse(std::shared_ptr<HttpConnection> conn, bool keepAlive, bool headersOnly, bool gzipAllowed)
        : connection(std::move(conn))
        , keepAlive(keepAlive)
        , headersOnly(headersOnly)
        , gzipAllowed(gzipAllowed)
    {
    }

    void setStatus(int code) override
    {
        if (!finished)
        {
            statusCode = code;
        }
    }

    void setHeader(const std::string& name, const std::string& value) override
    {
        if (!finished)
        {
            headerMap[ReactorHelpers::sanitizeHeader(name)] = ReactorHelpers::sanitizeHeader(value);
        }
    }

    void addHeader(const std::string& name, const std::string& value) override
    {
        if (!finished)
        {
            extraHeaders.emplace_back(ReactorHelpers::sanitizeHeader(name), ReactorHelpers::sanitizeHeader(value));
        }
    }

    void write(const std::string& chunk) override
    {
        if (!finished)
        {
            bodyBuffer += chunk;
        }
    }

    void end(const std::string& body) override;
    void sendFile(const std::string& path, std::uint64_t start, std::uint64_t length, bool headersOnly) override;

    void beginChunked() override;
    void writeChunk(const std::string& chunk) override;
    void endChunked() override;

    bool ended() const override
    {
        return finished;
    }

private:
    std::string buildHead(std::size_t contentLength, bool emitContentLength);
    std::string chunkedHead();
    void maybeCompress();

    std::shared_ptr<HttpConnection> connection;
    bool keepAlive;
    bool headersOnly;
    bool gzipAllowed;
    bool finished = false;
    bool chunkedMode = false;
    bool chunkedHeadSent = false;
    int statusCode = 200;
    std::map<std::string, std::string> headerMap;
    std::vector<std::pair<std::string, std::string>> extraHeaders;
    std::string bodyBuffer;
};

// one connection state machine, multiplexed on the reactor i/o thread and kept alive by the handlers and the response that reference it
class HttpConnection final : public std::enable_shared_from_this<HttpConnection>, public WebSocketConnection
{
public:
    HttpConnection(Poco::Net::StreamSocket socket, EventLoop& loop, Runtime& runtime, HttpServerOptions opts, HttpHandler handler, WebSocketHandler onWebSocket, std::shared_ptr<StaticFileHandler> staticFiles, std::shared_ptr<std::atomic<bool>> stopping)
        : socket(std::move(socket))
        , loop(loop)
        , runtime(runtime)
        , options(std::move(opts))
        , handler(std::move(handler))
        , onWebSocket(std::move(onWebSocket))
        , staticFiles(std::move(staticFiles))
        , stopping(std::move(stopping))
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

    ~HttpConnection()
    {
        closeFileFd();
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
        if (!dispatched && writeBuffer.empty() && (now - requestStartMs) > timeoutMs)
        {
            return true;
        }

        // a response that has stopped draining for the timeout is a slow-read client holding the slot open
        return !writeBuffer.empty() && (now - lastWriteMs) > timeoutMs;
    }

    void forceClose()
    {
        closeNow();
    }

    void submitResponse(std::string data, bool keepAliveAfter)
    {
        // drop the response if the connection was already force-closed while the handler was still awaiting
        if (closed)
        {
            return;
        }

        writeBuffer = std::move(data);
        writeOffset = 0;
        lastWriteMs = ReactorHelpers::nowMs();
        keepAlive = keepAliveAfter;
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->onWritable(); });
    }

    void submitStreamChunk(std::string data, bool keepAliveAfter, bool last)
    {
        // a streaming response appends each chunk to the unflushed tail and arms a single writer, only finishing the connection on the last chunk
        if (closed)
        {
            return;
        }

        // drop a peer that has stopped draining so a stalled client cannot grow the backlog without bound
        if ((writeBuffer.size() - writeOffset) + data.size() > kStreamMaxOutBytes)
        {
            closeNow();
            return;
        }

        writeBuffer.append(data);
        streamingActive = !last;
        keepAlive = keepAliveAfter;

        // a writer is already draining the buffer, so the appended bytes ride along without a second queued handler
        if (streamWriteArmed)
        {
            return;
        }

        // the writer starts draining now, so the staleness clock begins here and only a real flush advances it afterwards
        lastWriteMs = ReactorHelpers::nowMs();
        streamWriteArmed = true;
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->onWritable(); });
    }

    void streamFile(std::string head, std::string path, std::uint64_t start, std::uint64_t length, bool headersOnly, bool keepAliveAfter)
    {
        // drop the response if the connection was already force-closed while the handler was still awaiting
        if (closed)
        {
            return;
        }

        writeBuffer = std::move(head);
        writeOffset = 0;
        lastWriteMs = ReactorHelpers::nowMs();
        keepAlive = keepAliveAfter;

        // a head-only response or an empty range has no body to stream after the headers
        fileMode = !headersOnly && length > 0;
        if (fileMode)
        {
            filePath = std::move(path);
            fileOffset = start;
            fileRemaining = length;
        }

        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->onWritable(); });
    }

    void accept() override
    {
        if (wsMode || closed)
        {
            return;
        }

        std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Connection: Upgrade\r\n";
        response += "Sec-WebSocket-Accept: " + ReactorHelpers::computeWsAccept(wsKey) + "\r\n\r\n";

        // drop the consumed handshake request so the frame parser starts on client frame bytes
        readBuffer.erase(0, wsRequestEnd);

        wsMode = true;
        keepAlive = true;
        writeBuffer = std::move(response);
        writeOffset = 0;
        lastWriteMs = ReactorHelpers::nowMs();
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->onWritable(); });
    }

    void reject(int statusCode) override
    {
        sendSimpleAndClose(statusCode);
    }

    void send(const std::string& message) override
    {
        if (!closed)
        {
            wsSend(kWsText, message);
        }
    }

    void close() override
    {
        if (closed || wsCloseAfterFlush)
        {
            return;
        }

        std::string payload;
        payload.push_back(static_cast<char>((1000 >> 8) & 0xFF));
        payload.push_back(static_cast<char>(1000 & 0xFF));
        wsSend(kWsClose, payload);
        wsCloseAfterFlush = true;
    }

    void onMessage(std::function<void(const std::string&)> handler) override
    {
        wsMessageHandler = std::move(handler);
    }

    void onClose(std::function<void()> handler) override
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
        Error
    };

    // an i/o attempt either wants the same direction again, wants the opposite direction (tls renegotiation or handshake), or is done with the connection
    enum class Io
    {
        Again,
        Switch,
        Detached
    };

    void armRead()
    {
        auto self = shared_from_this();
        loop.watchRead(socket, [self]() -> bool
                       { return self->onReadable(); });
    }

    void armWrite()
    {
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

        if (wsCloseHandler)
        {
            auto handler = std::move(wsCloseHandler);
            wsCloseHandler = nullptr;
            handler();
        }

        if (loop.isRunning())
        {
            loop.closeSocket(socket);
            return;
        }

        try
        {
            socket.impl()->close();
        }
        catch (...)
        {
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
            // the tls layer needs the socket writable before this read can progress
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

    Io tryRead()
    {
        char buffer[kReadChunk];
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

        if (draining)
        {
            const std::size_t consumed = std::min(static_cast<std::size_t>(received), drainRemaining);
            drainRemaining -= consumed;
            if (drainRemaining == 0)
            {
                closeNow();
                return Io::Detached;
            }

            return Io::Again;
        }

        // a streaming response only watches the read side to learn the client left, ignoring any bytes it sends mid-stream
        if (streamingActive)
        {
            return Io::Again;
        }

        readBuffer.append(buffer, static_cast<std::size_t>(received));
        return process() == Progress::Detached ? Io::Detached : Io::Again;
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
            // the tls layer needs the socket readable before this write can progress
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
        while (writeOffset < writeBuffer.size())
        {
            const std::size_t remaining = writeBuffer.size() - writeOffset;
            const int chunk = static_cast<int>(std::min(remaining, static_cast<std::size_t>(INT_MAX)));
            int wrote;
            try
            {
                wrote = socket.sendBytes(writeBuffer.data() + writeOffset, chunk);
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
                ReactorHelpers::compactSent(writeBuffer, writeOffset);
                return Io::Again;
            }

            if (wrote == 0)
            {
                closeNow();
                return Io::Detached;
            }

            writeOffset += static_cast<std::size_t>(wrote);
            lastWriteMs = ReactorHelpers::nowMs();
        }

        // a file response sends its body before the connection's keep-alive fate is decided
        if (fileMode && fileRemaining > 0)
        {
#if defined(VARN_HAS_SENDFILE)
            // tls must encrypt in user space, so only a plaintext socket can hand the file straight to the kernel
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

                // an early eof or a transfer error ends the body, so the connection cannot be reused
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

        // the rejection response is flushed, but the connection stays open until the read side has drained the upload
        if (draining)
        {
            return Io::Detached;
        }

        // once the websocket handshake response is flushed the connection switches to reading client frames
        if (wsMode)
        {
            armRead();

            // drain any frames the client pipelined with the handshake instead of waiting for the next readable event
            if (!readBuffer.empty())
            {
                wsProcess();
            }

            return Io::Detached;
        }

        // a streaming response has flushed this chunk and waits for the next one while watching the read side for a client disconnect
        if (streamingActive)
        {
            writeBuffer.clear();
            writeOffset = 0;
            streamWriteArmed = false;

            // arm the disconnect watcher once since the reader re-pushes itself for the whole stream
            if (!streamReadArmed)
            {
                streamReadArmed = true;
                armRead();
            }

            return Io::Detached;
        }

        if (!keepAlive || stopping->load(std::memory_order_acquire))
        {
            closeNow();
            return Io::Detached;
        }

        // a kept-alive connection preserves any pipelined bytes and either parses the next request now or waits for more
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

            if (decoded == ChunkResult::Error)
            {
                sendSimpleAndClose(400);
                return Progress::Detached;
            }

            requestBody = std::move(chunkedBody);
        }
        else
        {
            // reject a content-length that would overflow the offset math regardless of whether the body cap is enabled
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
                // an unterminated chunk-size line past the cap is a framing attack rather than a slow client
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

            // reject a declared size that overflows the offset math or exceeds the body cap before the chunk is buffered
            if (chunkSize > readBuffer.max_size() - dataStart - 2)
            {
                return ChunkResult::Error;
            }

            if (options.maxRequestBodyBytes > 0 && chunkSize > static_cast<std::size_t>(options.maxRequestBodyBytes) - chunkedBody.size())
            {
                return ChunkResult::Error;
            }

            // a chunk needs its declared bytes plus the trailing crlf before it can be consumed
            if (readBuffer.size() < dataStart + chunkSize + 2)
            {
                return ChunkResult::NeedMore;
            }

            chunkedBody.append(readBuffer, dataStart, chunkSize);
            if (options.maxRequestBodyBytes > 0 && chunkedBody.size() > static_cast<std::size_t>(options.maxRequestBodyBytes))
            {
                return ChunkResult::Error;
            }

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

            // bound the whole trailer section so a flood of tiny trailer lines cannot grow the buffer unbounded or force a quadratic rescan
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

        // llhttp parses the request line and headers in place and rejects smuggling, duplicate content-length, and malformed framing
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
            // only chunked is supported, and it must not be combined with a content-length
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

            if (options.maxRequestBodyBytes > 0 && contentLength > static_cast<std::size_t>(options.maxRequestBodyBytes))
            {
                beginRejectDrain(413);
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

        // split the target into path and query in place, decoding only the parts that carry escapes
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

        auto response = std::make_shared<ReactorResponse>(shared_from_this(), keepAlive, request.method == "HEAD", gzipAllowed);

        // a matching public file is served ahead of the user handler and streamed off the loop
        if (staticFiles && staticFiles->tryServe(request, *response))
        {
            return;
        }

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
            const WsParse result = ReactorHelpers::parseWsFrame(readBuffer, frame);
            if (result == WsParse::NeedMore)
            {
                return Progress::NeedMore;
            }

            if (result == WsParse::Error)
            {
                closeNow();
                return Progress::Detached;
            }

            readBuffer.erase(0, frame.consumed);
            if (!handleWsFrame(frame))
            {
                return Progress::Detached;
            }
        }
    }

    bool handleWsFrame(const WsFrame& frame)
    {
        const bool isControl = (frame.opcode & 0x08) != 0;

        // a control frame must be final and at most 125 bytes, and only defined opcodes are accepted
        if (isControl)
        {
            const bool knownControl = frame.opcode == kWsClose || frame.opcode == kWsPing || frame.opcode == kWsPong;
            if (!knownControl || !frame.fin || frame.payload.size() > 125)
            {
                closeNow();
                return false;
            }
        }
        else if (frame.opcode != kWsContinuation && frame.opcode != kWsText && frame.opcode != kWsBinary)
        {
            closeNow();
            return false;
        }

        if (frame.opcode == kWsClose)
        {
            // a close payload is either empty or a 2-byte code plus optional reason, so a single byte is malformed
            if (frame.payload.size() == 1)
            {
                closeNow();
                return false;
            }

            // echo the close and let the writer close the socket once it flushes so the close handshake completes
            wsSend(kWsClose, frame.payload);
            wsCloseAfterFlush = true;
            return false;
        }

        if (frame.opcode == kWsPing)
        {
            wsSend(kWsPong, frame.payload);
            return !closed;
        }

        if (frame.opcode == kWsPong)
        {
            return true;
        }

        // data frames assemble across fragments into one message, enforcing the continuation state machine
        const bool isContinuation = frame.opcode == kWsContinuation;
        if (isContinuation != wsFragmentOpen)
        {
            // a continuation without an open message, or a fresh data frame while one is still open, violates the framing
            closeNow();
            return false;
        }

        if (!isContinuation)
        {
            wsMessage.clear();
        }

        if (wsMessage.size() + frame.payload.size() > kWsMaxMessageBytes)
        {
            closeNow();
            return false;
        }

        wsMessage += frame.payload;
        wsFragmentOpen = !frame.fin;
        if (frame.fin)
        {
            if (wsMessageHandler)
            {
                wsMessageHandler(wsMessage);
            }

            wsMessage.clear();
        }

        return true;
    }

    void wsSend(int opcode, const std::string& payload)
    {
        const std::string frame = ReactorHelpers::buildWsFrame(opcode, payload);

        // close instead of buffering without bound when the peer stops draining its socket
        if ((wsOut.size() - wsOutOffset) + frame.size() > kWsMaxOutBytes)
        {
            closeNow();
            return;
        }

        wsOut += frame;
        if (wsWriting)
        {
            return;
        }

        wsWriting = true;
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->wsOnWritable(); });
    }

    bool wsOnWritable()
    {
        while (wsOutOffset < wsOut.size())
        {
            const std::size_t remaining = wsOut.size() - wsOutOffset;
            const int chunk = static_cast<int>(std::min(remaining, static_cast<std::size_t>(INT_MAX)));
            int wrote;
            try
            {
                wrote = socket.sendBytes(wsOut.data() + wsOutOffset, chunk);
            }
            catch (...)
            {
                closeNow();
                return true;
            }

            if (wrote < 0)
            {
                ReactorHelpers::compactSent(wsOut, wsOutOffset);
                return false;
            }

            if (wrote == 0)
            {
                closeNow();
                return true;
            }

            wsOutOffset += static_cast<std::size_t>(wrote);
        }

        wsOut.clear();
        wsOutOffset = 0;
        wsWriting = false;
        if (wsCloseAfterFlush)
        {
            closeNow();
        }

        return true;
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

        // a short read means the file ended before its declared length, so the stream stops and the connection closes
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

        writeBuffer = std::move(chunk);
        writeOffset = 0;
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->onWritable(); });
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
        const std::string body = std::string(ReactorHelpers::reasonPhrase(code)) + ".";
        std::string head = "HTTP/1.1 " + std::to_string(code) + " " + ReactorHelpers::reasonPhrase(code) + "\r\n";
        head += "Content-Type: text/plain; charset=utf-8\r\n";
        head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        head += "Connection: close\r\n\r\n";
        head += body;
        submitResponse(std::move(head), false);
    }

    void beginRejectDrain(int code)
    {
        // reject the oversized upload but read off the rest of the body first, so the close is a clean fin rather than a reset that loses the response
        const std::size_t received = readBuffer.size() - bodyStart;
        drainRemaining = contentLength > received ? contentLength - received : 0;
        readBuffer.clear();

        if (drainRemaining == 0)
        {
            sendSimpleAndClose(code);
            return;
        }

        draining = true;
        const std::string body = std::string(ReactorHelpers::reasonPhrase(code)) + ".";
        std::string head = "HTTP/1.1 " + std::to_string(code) + " " + ReactorHelpers::reasonPhrase(code) + "\r\n";
        head += "Content-Type: text/plain; charset=utf-8\r\n";
        head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        head += "Connection: close\r\n\r\n";
        head += body;
        keepAlive = false;
        writeBuffer = std::move(head);
        writeOffset = 0;
        lastWriteMs = ReactorHelpers::nowMs();
        auto self = shared_from_this();
        loop.watchWrite(socket, [self]() -> bool
                        { return self->onWritable(); });
        armRead();
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
        writeBuffer.clear();
        writeOffset = 0;
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
    std::shared_ptr<std::atomic<bool>> stopping;
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

    std::string writeBuffer;
    std::size_t writeOffset = 0;

    bool fileMode = false;
    std::string filePath;
    std::uint64_t fileOffset = 0;
    std::uint64_t fileRemaining = 0;
#if defined(VARN_HAS_SENDFILE)
    int fileFd = -1;
#endif

    bool wsMode = false;
    std::string wsMessage;
    bool wsFragmentOpen = false;
    std::string wsOut;
    std::size_t wsOutOffset = 0;
    bool wsWriting = false;
    bool wsCloseAfterFlush = false;
    std::string wsKey;
    std::size_t wsRequestEnd = 0;
    std::function<void(const std::string&)> wsMessageHandler;
    std::function<void()> wsCloseHandler;

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

std::string ReactorResponse::buildHead(std::size_t contentLength, bool emitContentLength)
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

    if (emitContentLength)
    {
        head += "Content-Length: ";
        head += std::to_string(contentLength);
        head += "\r\n";
    }

    head += keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    head += "\r\n";
    return head;
}

void ReactorResponse::maybeCompress()
{
#if defined(VARN_HAVE_ZLIB)
    if (!gzipAllowed || bodyBuffer.size() < kCompressThreshold)
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
    if (!ReactorHelpers::gzipEncode(bodyBuffer, compressed) || compressed.size() >= bodyBuffer.size())
    {
        return;
    }

    bodyBuffer = std::move(compressed);
    headerMap["Content-Encoding"] = "gzip";
    extraHeaders.emplace_back("Vary", "Accept-Encoding");
#endif
}

void ReactorResponse::end(const std::string& body)
{
    if (finished)
    {
        return;
    }

    // a streaming response finishes through its chunked terminator, so a stray end flushes the last data as a chunk instead of a framed body
    if (chunkedMode)
    {
        if (!body.empty())
        {
            writeChunk(body);
        }

        endChunked();
        return;
    }

    finished = true;
    bodyBuffer += body;

    const bool bodyless = statusCode == 204 || statusCode == 304;
    if (!bodyless)
    {
        maybeCompress();
    }

    std::string head = buildHead(bodyBuffer.size(), !bodyless);

    // a head request still advertises Content-Length but carries no body
    if (!bodyless && !headersOnly)
    {
        head += bodyBuffer;
    }

    connection->submitResponse(std::move(head), keepAlive);
}

void ReactorResponse::beginChunked()
{
    if (finished || chunkedMode)
    {
        return;
    }

    chunkedMode = true;
}

void ReactorResponse::writeChunk(const std::string& chunk)
{
    if (finished)
    {
        return;
    }

    if (!chunkedMode)
    {
        beginChunked();
    }

    std::string frame = chunkedHead();

    // an empty write carries no chunk, since a zero-length chunk would terminate the stream early
    if (!chunk.empty() && !headersOnly)
    {
        std::ostringstream size;
        size << std::hex << chunk.size();
        frame += size.str();
        frame += "\r\n";
        frame += chunk;
        frame += "\r\n";
    }

    if (!frame.empty())
    {
        connection->submitStreamChunk(std::move(frame), keepAlive, false);
    }
}

void ReactorResponse::endChunked()
{
    if (finished)
    {
        return;
    }

    finished = true;

    std::string frame = chunkedHead();
    frame += "0\r\n\r\n";
    connection->submitStreamChunk(std::move(frame), keepAlive, true);
}

std::string ReactorResponse::chunkedHead()
{
    if (chunkedHeadSent)
    {
        return std::string();
    }

    chunkedHeadSent = true;

    // the head leads the stream once with chunked transfer encoding in place of a content length
    std::string head = buildHead(0, false);
    if (head.size() >= 2)
    {
        head.erase(head.size() - 2);
    }

    head += "Transfer-Encoding: chunked\r\n\r\n";
    return head;
}

void ReactorResponse::sendFile(const std::string& path, std::uint64_t start, std::uint64_t length, bool headersOnly)
{
    if (finished)
    {
        return;
    }

    finished = true;

    const bool emitContentLength = statusCode != 204 && statusCode != 304;
    std::string head = buildHead(static_cast<std::size_t>(length), emitContentLength);
    connection->streamFile(std::move(head), path, start, length, headersOnly, keepAlive);
}

void ReactorHelpers::scheduleSweep(EventLoop& loop, std::shared_ptr<std::vector<std::weak_ptr<HttpConnection>>> registry, std::shared_ptr<std::atomic<bool>> stopping, long long timeoutMs)
{
    // clang-format off
    loop.postDelayed(kSweepIntervalMs, [&loop, registry, stopping, timeoutMs]()
    {
        if (stopping->load(std::memory_order_acquire))
        {
            // force-close every live connection so its close handler runs and releases its lua reference, instead of leaking on shutdown
            for (auto& weak : *registry)
            {
                if (auto connection = weak.lock())
                {
                    connection->forceClose();
                }
            }

            registry->clear();
            return;
        }

        // close connections still idle-receiving past the deadline and compact the registry in place
        const long long now = ReactorHelpers::nowMs();
        auto& connections = *registry;
        std::size_t kept = 0;
        for (std::size_t i = 0; i < connections.size(); ++i)
        {
            auto connection = connections[i].lock();
            if (!connection || connection->isClosed())
            {
                continue;
            }

            // a non-positive timeout keeps connections open, so the sweep only compacts the registry then
            if (timeoutMs > 0 && connection->isStale(now, timeoutMs))
            {
                connection->forceClose();
                continue;
            }

            connections[kept++] = connections[i];
        }

        connections.resize(kept);

        ReactorHelpers::scheduleSweep(loop, registry, stopping, timeoutMs);
    });
    // clang-format on
}

} // namespace

ReactorHttpServer::ReactorHttpServer(Runtime& rt, HttpServerOptions opts, HttpHandler onRequest, WebSocketHandler onWebSocket)
    : runtime(rt)
    , serverOptions(std::move(opts))
    , httpHandler(std::move(onRequest))
    , webSocketHandler(std::move(onWebSocket))
{
}

ReactorHttpServer::~ReactorHttpServer()
{
    stop();
}

void ReactorHttpServer::start()
{
    if (started)
    {
        return;
    }

    started = true;
    stopping->store(false, std::memory_order_release);

    EventLoop& loop = runtime.mainLoop();

    const Poco::Net::SocketAddress address(serverOptions.host, static_cast<Poco::UInt16>(serverOptions.port));
    const int backlog = std::clamp(serverOptions.maxQueued, 1, 65535);

    // bind with so_reuseport so every worker process shares the same listening port and the kernel load-balances accepts
#ifdef VARN_ENABLE_TLS
    if (serverOptions.tls)
    {
        Poco::Net::Context::Ptr context = TlsServerContext::create(serverOptions);
        TlsServerContext::initializeSslManager(context);
        Poco::Net::SecureServerSocket secure(context);
        secure.bind(address, true, true);
        secure.listen(backlog);
        listener = secure;
    }
    else
    {
        Poco::Net::ServerSocket plain;
        plain.bind(address, true, true);
        plain.listen(backlog);
        listener = plain;
    }
#else
    Poco::Net::ServerSocket plain;
    plain.bind(address, true, true);
    plain.listen(backlog);
    listener = plain;
#endif

    listener.setBlocking(false);

#if defined(__linux__)
    // defer accept until the client's first request bytes arrive, so the first read always has data and connect-only floods never reach the loop
    {
        const int deferSeconds = std::clamp(serverOptions.keepAliveTimeoutSeconds, 1, 600);
        ::setsockopt(listener.impl()->sockfd(), IPPROTO_TCP, TCP_DEFER_ACCEPT, &deferSeconds, sizeof(deferSeconds));
    }
#endif

    auto registry = std::make_shared<std::vector<std::weak_ptr<HttpConnection>>>();
    const long long timeoutMs = static_cast<long long>(serverOptions.keepAliveTimeoutSeconds) * 1000;

    std::shared_ptr<StaticFileHandler> staticFiles;
    if (serverOptions.servePublic)
    {
        staticFiles = std::make_shared<StaticFileHandler>(serverOptions.publicDir, serverOptions.directoryListing);
    }

    Runtime& rt = runtime;
    HttpServerOptions opts = serverOptions;
    HttpHandler onRequest = httpHandler;
    WebSocketHandler onWebSocket = webSocketHandler;
    auto stop = stopping;
    Poco::Net::ServerSocket server = listener;

    // clang-format off
    loop.watchRead(listener, [&loop, &rt, opts, onRequest, onWebSocket, staticFiles, stop, server, registry]() mutable -> bool
    {
        if (stop->load(std::memory_order_acquire))
        {
            return true;
        }

        // drain every pending connection so a single readiness event accepts the full backlog
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

            auto connection = std::make_shared<HttpConnection>(std::move(accepted), loop, rt, opts, onRequest, onWebSocket, staticFiles, stop);
            registry->push_back(connection);
            connection->start();
        }

        return false;
    });
    // clang-format on

    // the sweep also compacts the connection registry, so it runs even when the idle and slowloris timeout is disabled
    ReactorHelpers::scheduleSweep(loop, registry, stopping, timeoutMs);
}

void ReactorHttpServer::stop()
{
    if (!started)
    {
        return;
    }

    started = false;
    stopping->store(true, std::memory_order_release);
    EventLoop& loop = runtime.mainLoop();
    if (loop.isRunning())
    {
        loop.closeSocket(listener);
    }
    else
    {
        try
        {
            listener.impl()->close();
        }
        catch (...)
        {
        }
    }
}

} // namespace varn::http
