#include "varn/http/HttpTypes.h"
#include "varn/http/drivers/reactor/ReactorHttpServer.h"
#include "varn/runtime/Runtime.h"

#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Timespan.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace varn::http
{

namespace
{

// Drives a server on the loop of a runtime from the test thread while a blocking client talks to it from another thread.
class HttpServerTest : public ::testing::Test
{
protected:
    struct Frame
    {
        int opcode = -1;
        std::string payload;
    };

    // Records what the connection of a WebSocket test reported, all of it on the loop thread.
    struct WsRecord
    {
        std::shared_ptr<WebSocketConnection> conn;
        std::vector<std::pair<std::string, bool>> messages;
        std::vector<std::string> pongs;
        int closeCode = 0;
        std::string closeReason;
        bool closed = false;
    };

    static HttpServerOptions localOptions()
    {
        HttpServerOptions options;
        options.host = "127.0.0.1";
        options.port = 0;
        return options;
    }

    bool pumpUntil(const std::function<bool()>& condition, std::chrono::milliseconds limit = std::chrono::seconds(10))
    {
        const auto end = std::chrono::steady_clock::now() + limit;
        while (!condition())
        {
            if (std::chrono::steady_clock::now() >= end)
            {
                return false;
            }

            runtime.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }

    template <typename T>
    bool pumpFor(std::future<T>& future, std::chrono::milliseconds limit = std::chrono::seconds(10))
    {
        // clang-format off
        return pumpUntil([&future]
        {
            return future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        }, limit);
        // clang-format on
    }

    void idle(std::chrono::milliseconds length)
    {
        // clang-format off
        pumpUntil([] { return false; }, length);
        // clang-format on
    }

    static Poco::Net::StreamSocket connect(int port)
    {
        Poco::Net::StreamSocket socket(Poco::Net::SocketAddress("127.0.0.1", static_cast<Poco::UInt16>(port)));
        socket.setReceiveTimeout(Poco::Timespan(10, 0));
        return socket;
    }

    // Sends the bytes and reads until the head of an answer arrived, or until the server closed when asked to.
    static std::string exchange(Poco::Net::StreamSocket& socket, const std::string& bytes, bool untilClosed)
    {
        socket.sendBytes(bytes.data(), static_cast<int>(bytes.size()));

        std::string answer;
        char buffer[65536];
        while (untilClosed || answer.find("\r\n\r\n") == std::string::npos)
        {
            const int received = socket.receiveBytes(buffer, sizeof(buffer));
            if (received <= 0)
            {
                break;
            }

            answer.append(buffer, static_cast<std::size_t>(received));
        }

        return answer;
    }

    static std::string get(int port, const std::string& path)
    {
        Poco::Net::StreamSocket socket = connect(port);
        return exchange(socket, "GET " + path + " HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n", true);
    }

    // Builds a client frame masked with a zero key, which leaves the payload as it is.
    static std::string clientFrame(int opcode, const std::string& payload)
    {
        std::string frame;
        frame.push_back(static_cast<char>(0x80 | opcode));
        if (payload.size() < 126)
        {
            frame.push_back(static_cast<char>(0x80 | payload.size()));
        }
        else
        {
            frame.push_back(static_cast<char>(0x80 | 126));
            frame.push_back(static_cast<char>((payload.size() >> 8) & 0xFF));
            frame.push_back(static_cast<char>(payload.size() & 0xFF));
        }

        frame.append(4, '\0');
        frame += payload;
        return frame;
    }

    static void sendFrame(Poco::Net::StreamSocket& socket, int opcode, const std::string& payload)
    {
        const std::string frame = clientFrame(opcode, payload);
        socket.sendBytes(frame.data(), static_cast<int>(frame.size()));
    }

    static bool readExactly(Poco::Net::StreamSocket& socket, std::string& buffer, std::size_t size)
    {
        char chunk[65536];
        while (buffer.size() < size)
        {
            const int received = socket.receiveBytes(chunk, sizeof(chunk));
            if (received <= 0)
            {
                return false;
            }

            buffer.append(chunk, static_cast<std::size_t>(received));
        }

        return true;
    }

    // Reads one server frame, keeping what follows it in the buffer, and answers an opcode of -1 once the server closed.
    static Frame readFrame(Poco::Net::StreamSocket& socket, std::string& buffer)
    {
        if (!readExactly(socket, buffer, 2))
        {
            return {};
        }

        std::size_t length = static_cast<unsigned char>(buffer[1]) & 0x7F;
        std::size_t offset = 2;
        if (length == 126)
        {
            if (!readExactly(socket, buffer, 4))
            {
                return {};
            }

            length = (static_cast<std::size_t>(static_cast<unsigned char>(buffer[2])) << 8) | static_cast<unsigned char>(buffer[3]);
            offset = 4;
        }
        else if (length == 127)
        {
            if (!readExactly(socket, buffer, 10))
            {
                return {};
            }

            length = 0;
            for (std::size_t i = 2; i < 10; ++i)
            {
                length = (length << 8) | static_cast<unsigned char>(buffer[i]);
            }

            offset = 10;
        }

        if (!readExactly(socket, buffer, offset + length))
        {
            return {};
        }

        Frame frame{static_cast<unsigned char>(buffer[0]) & 0x0F, buffer.substr(offset, length)};
        buffer.erase(0, offset + length);
        return frame;
    }

    static int closeCode(const Frame& frame)
    {
        return (static_cast<unsigned char>(frame.payload[0]) << 8) | static_cast<unsigned char>(frame.payload[1]);
    }

    // Opens a WebSocket and leaves in the buffer what followed the head of the answer.
    static Poco::Net::StreamSocket openWebSocket(int port, std::string& buffer)
    {
        Poco::Net::StreamSocket socket = connect(port);
        const std::string head = exchange(socket, "GET /ws HTTP/1.1\r\nHost: test\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n", false);
        if (head.find(" 101 ") == std::string::npos)
        {
            throw std::runtime_error("The handshake did not switch protocols.");
        }

        buffer = head.substr(head.find("\r\n\r\n") + 4);
        return socket;
    }

    // Starts a server whose WebSocket route accepts with the options and records what its connection reports.
    std::unique_ptr<ReactorHttpServer> webSocketServer(WebSocketOptions settings, const std::shared_ptr<WsRecord>& record)
    {
        // clang-format off
        // The callbacks reach the record weakly, since the record holds the connection and the connection holds its callbacks.
        std::weak_ptr<WsRecord> weak = record;
        auto onWebSocket = [settings, weak](const HttpRequest&, std::shared_ptr<WebSocketConnection> conn)
        {
            if (auto owner = weak.lock())
            {
                owner->conn = conn;
            }

            conn->accept(settings);
            conn->onMessage([weak](const std::string& message, bool binary)
            {
                if (auto owner = weak.lock())
                {
                    owner->messages.emplace_back(message, binary);
                }
            });
            conn->onPong([weak](const std::string& payload)
            {
                if (auto owner = weak.lock())
                {
                    owner->pongs.push_back(payload);
                }
            });
            conn->onClose([weak](int code, const std::string& reason)
            {
                if (auto owner = weak.lock())
                {
                    owner->closed = true;
                    owner->closeCode = code;
                    owner->closeReason = reason;
                }
            });
        };
        // clang-format on

        auto server = std::make_unique<ReactorHttpServer>(runtime, localOptions(), HttpHandler{}, std::move(onWebSocket));
        server->start();
        return server;
    }

    varn::runtime::Runtime runtime{{"http_server_test"}};
};

} // namespace

TEST_F(HttpServerTest, BindsPortZeroAndTellsTheBoundPort)
{
    // clang-format off
    ReactorHttpServer server(runtime, localOptions(), [](HttpRequest, std::shared_ptr<HttpResponse> response)
    {
        response->end("bound");
    });
    // clang-format on
    server.start();
    ASSERT_GT(server.port(), 0);
    EXPECT_EQ(server.host(), "127.0.0.1");

    const int port = server.port();
    auto answer = std::async(std::launch::async, [port]
                             { return get(port, "/"); });
    ASSERT_TRUE(pumpFor(answer));
    EXPECT_NE(answer.get().find("\r\n\r\nbound"), std::string::npos);
}

TEST_F(HttpServerTest, RefusesABusyPortWithAMessageThatNamesIt)
{
    ReactorHttpServer first(runtime, localOptions(), HttpHandler{});
    first.start();

    HttpServerOptions taken = localOptions();
    taken.port = first.port();
    ReactorHttpServer second(runtime, taken, HttpHandler{});
    try
    {
        second.start();
        FAIL() << "A second server on a busy port must fail.";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("The address 127.0.0.1:" + std::to_string(first.port()) + " is already in use."), std::string::npos) << error.what();
    }
}

TEST_F(HttpServerTest, SharesAPortWhenBothServersAskTo)
{
    HttpServerOptions shared = localOptions();
    shared.reusePort = true;
    ReactorHttpServer first(runtime, shared, HttpHandler{});
    first.start();

    shared.port = first.port();
    ReactorHttpServer second(runtime, shared, HttpHandler{});
    EXPECT_NO_THROW(second.start());
    EXPECT_EQ(second.port(), first.port());
}

TEST_F(HttpServerTest, AnswersGatewayTimeoutPastTheRequestDeadline)
{
    HttpServerOptions options = localOptions();
    options.requestTimeoutMs = 100;
    std::shared_ptr<HttpResponse> held;
    // clang-format off
    ReactorHttpServer server(runtime, options, [&held](HttpRequest, std::shared_ptr<HttpResponse> response)
    {
        held = std::move(response);
    });
    // clang-format on
    server.start();

    const int port = server.port();
    auto answer = std::async(std::launch::async, [port]
                             { return get(port, "/"); });
    ASSERT_TRUE(pumpFor(answer));
    EXPECT_EQ(answer.get().rfind("HTTP/1.1 504", 0), 0u);

    // The handler that answers late finds its response ended.
    ASSERT_TRUE(held);
    EXPECT_TRUE(held->ended());
    held->end("late");
}

TEST_F(HttpServerTest, AnswersPayloadTooLargeFromTheHeadAndLetsTheClientGo)
{
    HttpServerOptions options = localOptions();
    options.maxRequestBodyBytes = 1024;
    bool handled = false;
    // clang-format off
    ReactorHttpServer server(runtime, options, [&handled](HttpRequest, std::shared_ptr<HttpResponse> response)
    {
        handled = true;
        response->end("read");
    });
    // clang-format on
    server.start();

    const int port = server.port();
    // clang-format off
    auto answer = std::async(std::launch::async, [port]
    {
        Poco::Net::StreamSocket socket = connect(port);
        const auto started = std::chrono::steady_clock::now();
        std::string text = exchange(socket, "POST / HTTP/1.1\r\nHost: test\r\nContent-Length: 104857600\r\n\r\n", true);
        const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
        return std::make_pair(text, waited);
    });
    // clang-format on
    ASSERT_TRUE(pumpFor(answer));

    const auto [text, waited] = answer.get();
    EXPECT_EQ(text.rfind("HTTP/1.1 413", 0), 0u) << text;
    EXPECT_NE(text.find("Connection: close"), std::string::npos);
    EXPECT_LT(waited.count(), 5000);
    EXPECT_FALSE(handled);
}

TEST_F(HttpServerTest, GracefulCloseWaitsForTheRequestInFlight)
{
    std::shared_ptr<HttpResponse> held;
    // clang-format off
    ReactorHttpServer server(runtime, localOptions(), [&held](HttpRequest, std::shared_ptr<HttpResponse> response)
    {
        held = std::move(response);
    });
    // clang-format on
    server.start();

    const int port = server.port();
    auto answer = std::async(std::launch::async, [port]
                             { return get(port, "/"); });
    ASSERT_TRUE(pumpUntil([&held]
                          { return held != nullptr; }));

    bool closed = false;
    server.close(true, 5000, [&closed]
                 { closed = true; });
    idle(std::chrono::milliseconds(100));
    EXPECT_FALSE(closed);

    held->end("finished");
    ASSERT_TRUE(pumpUntil([&closed]
                          { return closed; }));
    ASSERT_TRUE(pumpFor(answer));
    EXPECT_NE(answer.get().find("\r\n\r\nfinished"), std::string::npos);

    // clang-format off
    auto refused = std::async(std::launch::async, [port]
    {
        try
        {
            connect(port);
            return false;
        }
        catch (const Poco::Exception&)
        {
            return true;
        }
    });
    // clang-format on
    ASSERT_TRUE(pumpFor(refused));
    EXPECT_TRUE(refused.get());
}

TEST_F(HttpServerTest, CloseWithoutGraceDropsTheRequestInFlight)
{
    std::shared_ptr<HttpResponse> held;
    // clang-format off
    ReactorHttpServer server(runtime, localOptions(), [&held](HttpRequest, std::shared_ptr<HttpResponse> response)
    {
        held = std::move(response);
    });
    // clang-format on
    server.start();

    const int port = server.port();
    auto answer = std::async(std::launch::async, [port]
                             { return get(port, "/"); });
    ASSERT_TRUE(pumpUntil([&held]
                          { return held != nullptr; }));

    bool closed = false;
    server.close(false, 5000, [&closed]
                 { closed = true; });
    ASSERT_TRUE(pumpUntil([&closed]
                          { return closed; }, std::chrono::seconds(1)));
    ASSERT_TRUE(pumpFor(answer));
    EXPECT_EQ(answer.get(), "");
}

TEST_F(HttpServerTest, GracefulCloseEndsAtItsDeadline)
{
    std::shared_ptr<HttpResponse> held;
    // clang-format off
    ReactorHttpServer server(runtime, localOptions(), [&held](HttpRequest, std::shared_ptr<HttpResponse> response)
    {
        held = std::move(response);
    });
    // clang-format on
    server.start();

    const int port = server.port();
    auto answer = std::async(std::launch::async, [port]
                             { return get(port, "/"); });
    ASSERT_TRUE(pumpUntil([&held]
                          { return held != nullptr; }));

    bool closed = false;
    const auto started = std::chrono::steady_clock::now();
    server.close(true, 100, [&closed]
                 { closed = true; });
    ASSERT_TRUE(pumpUntil([&closed]
                          { return closed; }, std::chrono::seconds(3)));
    EXPECT_GE(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(90));
    ASSERT_TRUE(pumpFor(answer));
}

TEST_F(HttpServerTest, CloseSaysGoingAwayToEveryWebSocket)
{
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer({}, record);

    const int port = server->port();
    // clang-format off
    auto frame = std::async(std::launch::async, [port]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        return readFrame(socket, buffer);
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->conn != nullptr; }));

    bool closed = false;
    server->close(false, 5000, [&closed]
                  { closed = true; });
    ASSERT_TRUE(pumpUntil([&closed]
                          { return closed; }));
    ASSERT_TRUE(pumpFor(frame));

    const Frame goodbye = frame.get();
    ASSERT_EQ(goodbye.opcode, 0x8);
    EXPECT_EQ(closeCode(goodbye), 1001);
    EXPECT_TRUE(record->closed);
    EXPECT_EQ(record->closeCode, 1001);
}

TEST_F(HttpServerTest, WebSocketKeepsBinaryMessagesBinary)
{
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer({}, record);

    const int port = server->port();
    const std::string bytes("\x00\x01\xFF", 3);
    std::promise<Frame> echoed;
    auto echo = echoed.get_future();
    std::promise<void> sent;
    auto ready = sent.get_future().share();
    // clang-format off
    std::thread client([port, bytes, &echoed, ready]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        sendFrame(socket, 0x2, bytes);
        ready.wait();
        echoed.set_value(readFrame(socket, buffer));
    });
    // clang-format on

    ASSERT_TRUE(pumpUntil([&record]
                          { return !record->messages.empty(); }));
    EXPECT_EQ(record->messages[0].first, bytes);
    EXPECT_TRUE(record->messages[0].second);

    bool delivered = false;
    record->conn->send(std::make_shared<const std::string>(record->messages[0].first), true, [&delivered](bool done)
                       { delivered = done; });
    sent.set_value();
    ASSERT_TRUE(pumpFor(echo));
    client.join();

    const Frame frame = echo.get();
    EXPECT_EQ(frame.opcode, 0x2);
    EXPECT_EQ(frame.payload, bytes);
    ASSERT_TRUE(pumpUntil([&delivered]
                          { return delivered; }));
}

TEST_F(HttpServerTest, WebSocketClosesTextThatIsNotUtf8With1007)
{
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer({}, record);

    const int port = server->port();
    // clang-format off
    auto frame = std::async(std::launch::async, [port]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        sendFrame(socket, 0x1, "\xC3\x28");
        return readFrame(socket, buffer);
    });
    // clang-format on
    ASSERT_TRUE(pumpFor(frame));

    const Frame goodbye = frame.get();
    ASSERT_EQ(goodbye.opcode, 0x8);
    EXPECT_EQ(closeCode(goodbye), 1007);
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->closed; }));
    EXPECT_EQ(record->closeCode, 1007);
    EXPECT_TRUE(record->messages.empty());
}

TEST_F(HttpServerTest, WebSocketReportsTheCloseOfThePeerWithItsCodeAndReason)
{
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer({}, record);

    const int port = server->port();
    // clang-format off
    auto frame = std::async(std::launch::async, [port]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        sendFrame(socket, 0x8, std::string("\x0F\xA2", 2) + "later");
        return readFrame(socket, buffer);
    });
    // clang-format on
    ASSERT_TRUE(pumpFor(frame));

    const Frame answer = frame.get();
    ASSERT_EQ(answer.opcode, 0x8);
    EXPECT_EQ(closeCode(answer), 4002);
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->closed; }));
    EXPECT_EQ(record->closeCode, 4002);
    EXPECT_EQ(record->closeReason, "later");
}

TEST_F(HttpServerTest, WebSocketClosesWithTheCodeAndReasonTheServerChose)
{
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer({}, record);

    const int port = server->port();
    // clang-format off
    auto frame = std::async(std::launch::async, [port]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        return readFrame(socket, buffer);
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->conn != nullptr; }));
    record->conn->close(4000, "done");
    ASSERT_TRUE(pumpFor(frame));

    const Frame goodbye = frame.get();
    ASSERT_EQ(goodbye.opcode, 0x8);
    EXPECT_EQ(closeCode(goodbye), 4000);
    EXPECT_EQ(goodbye.payload.substr(2), "done");
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->closed; }));
    EXPECT_EQ(record->closeCode, 4000);
    EXPECT_EQ(record->closeReason, "done");
}

TEST_F(HttpServerTest, WebSocketPingsAndHearsThePong)
{
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer({}, record);

    const int port = server->port();
    // clang-format off
    auto frame = std::async(std::launch::async, [port]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        Frame ping = readFrame(socket, buffer);
        sendFrame(socket, 0xA, ping.payload);
        return ping;
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->conn != nullptr; }));
    record->conn->ping("probe");
    ASSERT_TRUE(pumpFor(frame));

    const Frame ping = frame.get();
    EXPECT_EQ(ping.opcode, 0x9);
    EXPECT_EQ(ping.payload, "probe");
    ASSERT_TRUE(pumpUntil([&record]
                          { return !record->pongs.empty(); }));
    EXPECT_EQ(record->pongs[0], "probe");
}

TEST_F(HttpServerTest, WebSocketDropsAPeerThatAnswersNoPing)
{
    WebSocketOptions settings;
    settings.pingIntervalMs = 50;
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer(settings, record);

    const int port = server->port();
    // clang-format off
    auto frames = std::async(std::launch::async, [port]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        std::vector<Frame> seen;
        for (Frame frame = readFrame(socket, buffer); frame.opcode != -1; frame = readFrame(socket, buffer))
        {
            seen.push_back(frame);
        }

        return seen;
    });
    // clang-format on
    ASSERT_TRUE(pumpFor(frames));

    const std::vector<Frame> seen = frames.get();
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].opcode, 0x9);
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->closed; }));
    EXPECT_EQ(record->closeCode, 1006);
    EXPECT_NE(record->closeReason.find("did not answer"), std::string::npos);
}

TEST_F(HttpServerTest, WebSocketClosesPastTheMessageLimitWith1009)
{
    WebSocketOptions settings;
    settings.maxMessageBytes = 8;
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer(settings, record);

    const int port = server->port();
    // clang-format off
    auto frame = std::async(std::launch::async, [port]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        sendFrame(socket, 0x1, "12345678");
        sendFrame(socket, 0x1, "123456789");
        return readFrame(socket, buffer);
    });
    // clang-format on
    ASSERT_TRUE(pumpFor(frame));

    const Frame goodbye = frame.get();
    ASSERT_EQ(goodbye.opcode, 0x8);
    EXPECT_EQ(closeCode(goodbye), 1009);
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->closed; }));
    ASSERT_EQ(record->messages.size(), 1u);
    EXPECT_EQ(record->messages[0].first, "12345678");
}

TEST_F(HttpServerTest, WebSocketQueueCountsPayloadsAndDropsAPeerPastItsLimit)
{
    WebSocketOptions settings;
    settings.maxQueuedBytes = 4096;
    auto record = std::make_shared<WsRecord>();
    auto server = webSocketServer(settings, record);

    const int port = server->port();
    std::promise<void> release;
    auto released = release.get_future().share();
    // clang-format off
    auto frames = std::async(std::launch::async, [port, released]
    {
        std::string buffer;
        Poco::Net::StreamSocket socket = openWebSocket(port, buffer);
        released.wait();
        std::vector<Frame> seen;
        for (Frame frame = readFrame(socket, buffer); frame.opcode != -1; frame = readFrame(socket, buffer))
        {
            seen.push_back(frame);
        }

        return seen;
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil([&record]
                          { return record->conn != nullptr; }));

    // A message of exactly the limit fits an empty queue, since the head of its frame does not count.
    std::vector<int> outcomes;
    // clang-format off
    const auto track = [&outcomes]
    {
        const std::size_t slot = outcomes.size();
        outcomes.push_back(-1);
        return [&outcomes, slot](bool sent) { outcomes[slot] = sent ? 1 : 0; };
    };
    // clang-format on
    record->conn->send(std::make_shared<const std::string>(4096, 'a'), false, track());
    EXPECT_EQ(record->conn->bufferedAmount(), 4096u);
    ASSERT_TRUE(pumpUntil([&outcomes]
                          { return outcomes[0] != -1; }));
    EXPECT_EQ(outcomes[0], 1);
    EXPECT_EQ(record->conn->bufferedAmount(), 0u);

    // Sends nobody waits for fill the queue, and the one past the limit drops the peer with every send still waiting.
    record->conn->send(std::make_shared<const std::string>(2048, 'b'), false, track());
    record->conn->send(std::make_shared<const std::string>(2048, 'c'), false, track());
    EXPECT_EQ(record->conn->bufferedAmount(), 4096u);
    record->conn->send(std::make_shared<const std::string>(1, 'd'), false, track());

    EXPECT_TRUE(record->closed);
    EXPECT_EQ(record->closeCode, 1006);
    EXPECT_NE(record->closeReason.find("stopped reading"), std::string::npos);
    EXPECT_EQ(outcomes, (std::vector<int>{1, 0, 0, 0}));
    EXPECT_EQ(record->conn->bufferedAmount(), 0u);

    release.set_value();
    ASSERT_TRUE(pumpFor(frames));
    const std::vector<Frame> seen = frames.get();
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].payload.size(), 4096u);
}

} // namespace varn::http
