#include "varn/http/HttpClient.h"
#include "varn/runtime/Runtime.h"

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/StreamCopier.h>
#include <Poco/ThreadPool.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace varn::http::client
{

namespace
{

// What the handlers of the test server share with the test that drives them.
struct ServerState
{
    std::atomic<std::uint64_t> endlessWritten{0};
    std::mutex mutex;
    std::condition_variable released;
    bool releaseSecond = false;
    std::string echoedBody;
    std::string transferEncoding;
};

// The body the range route serves, the same bytes on every request so a resumed download can be compared with a whole one.
class RangeBody
{
public:
    static const std::string& text()
    {
        static const std::string body = []
        {
            std::string out;
            for (int index = 0; index < 1000; ++index)
            {
                out.push_back(static_cast<char>('a' + index % 26));
            }

            return out;
        }();

        return body;
    }
};

class TestHandler : public Poco::Net::HTTPRequestHandler
{
public:
    explicit TestHandler(ServerState& state)
        : state(state)
    {
    }

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
    {
        const std::string path = request.getURI();

        if (path == "/hello")
        {
            response.setContentLength(5);
            response.send() << "hello";
            return;
        }

        if (path == "/paced")
        {
            paced(response);
            return;
        }

        if (path == "/endless")
        {
            endless(response);
            return;
        }

        if (path == "/large")
        {
            response.setContentLength64(2 * 1024 * 1024);
            std::ostream& out = response.send();
            const std::string piece(64 * 1024, 'x');
            for (int index = 0; index < 32 && out.good(); ++index)
            {
                out << piece;
            }
            return;
        }

        if (path == "/slow")
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            response.setContentLength(4);
            response.send() << "slow";
            return;
        }

        if (path == "/moved")
        {
            response.redirect("/hello", Poco::Net::HTTPResponse::HTTP_FOUND);
            return;
        }

        if (path == "/range")
        {
            ranged(request, response);
            return;
        }

        if (path == "/cached")
        {
            if (request.get("If-None-Match", "") == "\"v1\"")
            {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_MODIFIED);
                response.send();
                return;
            }

            response.set("ETag", "\"v1\"");
            response.setContentLength(6);
            response.send() << "cached";
            return;
        }

        if (path == "/echo")
        {
            std::string body;
            Poco::StreamCopier::copyToString(request.stream(), body);
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.echoedBody = body;
                state.transferEncoding = request.getTransferEncoding();
            }

            response.setContentLength64(static_cast<Poco::UInt64>(body.size()));
            response.send() << body;
            return;
        }

        response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
        response.setContentLength(0);
        response.send();
    }

private:
    // Sends the head and a first piece, then holds the second until the test saw the first, which only a client that streams can let happen.
    // The URL Loading System holds the first bytes of a body whose type it may sniff, so the route says it must not.
    void paced(Poco::Net::HTTPServerResponse& response)
    {
        response.setChunkedTransferEncoding(true);
        response.setContentType("text/plain");
        response.set("X-Content-Type-Options", "nosniff");
        response.set("X-Paced", "yes");
        std::ostream& out = response.send();
        out << "first";
        out.flush();

        std::unique_lock<std::mutex> lock(state.mutex);
        // clang-format off
        state.released.wait_for(lock, std::chrono::seconds(10), [this] { return state.releaseSecond; });
        // clang-format on
        lock.unlock();

        out << "second";
        out.flush();
    }

    // Writes until the client goes away or a bound far past anything a test reads, counting what it wrote.
    void endless(Poco::Net::HTTPServerResponse& response)
    {
        response.setChunkedTransferEncoding(true);
        std::ostream& out = response.send();
        const std::string piece(64 * 1024, 'e');

        for (int index = 0; index < 4096 && out.good(); ++index)
        {
            out << piece;
            out.flush();
            state.endlessWritten.fetch_add(piece.size());
        }
    }

    void ranged(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
    {
        const std::string& body = RangeBody::text();
        const std::string range = request.get("Range", "");
        const std::string prefix = "bytes=";
        if (range.rfind(prefix, 0) != 0 || range.back() != '-')
        {
            response.setContentLength64(static_cast<Poco::UInt64>(body.size()));
            response.send() << body;
            return;
        }

        const std::size_t first = std::stoul(range.substr(prefix.size(), range.size() - prefix.size() - 1));
        response.setStatus(Poco::Net::HTTPResponse::HTTP_PARTIAL_CONTENT);
        response.set("Content-Range", "bytes " + std::to_string(first) + "-" + std::to_string(body.size() - 1) + "/" + std::to_string(body.size()));
        response.setContentLength64(static_cast<Poco::UInt64>(body.size() - first));
        response.send() << body.substr(first);
    }

    ServerState& state;
};

class TestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    explicit TestHandlerFactory(ServerState& state)
        : state(state)
    {
    }

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
    {
        return new TestHandler(state);
    }

private:
    ServerState& state;
};

// What one request delivered, in the order it arrived.
struct Outcome
{
    std::vector<std::string> events;
    std::optional<ResponseHead> head;
    std::string body;
    std::vector<Progress> progress;
    bool completed = false;
    std::optional<Error> error;
};

class HttpClientTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("127.0.0.1", 0));
        port = socket.address().port();

        auto* params = new Poco::Net::HTTPServerParams;
        params->setKeepAlive(true);
        params->setMaxThreads(16);
        params->setMaxQueued(64);
        server = std::make_unique<Poco::Net::HTTPServer>(new TestHandlerFactory(state), socket, params);
        server->start();
    }

    // The server is destroyed only once every pool thread that served a connection went idle, since stopping it does not wait for them.
    void TearDown() override
    {
        client.reset();
        server->stopAll(true);
        Poco::ThreadPool::defaultPool().joinAll();
        server.reset();
    }

    std::string url(const std::string& path) const
    {
        return "http://127.0.0.1:" + std::to_string(port) + path;
    }

    // Sends a request whose callbacks record everything into the outcome it answers.
    Handle send(Request request, const std::shared_ptr<Outcome>& outcome, std::function<void(std::string_view)> onChunk = nullptr)
    {
        HttpClient::Callbacks callbacks;

        // clang-format off
        callbacks.onHead = [outcome](const ResponseHead& head)
        {
            outcome->events.push_back("head");
            outcome->head = head;
        };

        callbacks.onChunk = [outcome, onChunk](std::string_view chunk)
        {
            outcome->events.push_back("chunk");
            outcome->body.append(chunk);
            if (onChunk)
            {
                onChunk(chunk);
            }
        };

        callbacks.onProgress = [outcome](const Progress& progress)
        {
            outcome->progress.push_back(progress);
        };

        callbacks.onComplete = [outcome](const std::optional<Error>& error)
        {
            outcome->events.push_back("complete");
            outcome->completed = true;
            outcome->error = error;
        };
        // clang-format on

        return client->send(std::move(request), std::move(callbacks));
    }

    // Polls the runtime the way a host polls it once per frame, until the condition holds or the time runs out.
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

    // Pumps until the request of the outcome completed.
    bool settles(const std::shared_ptr<Outcome>& outcome)
    {
        // clang-format off
        return pumpUntil([&] { return outcome->completed; });
        // clang-format on
    }

    // Pumps until the first piece of the body of the outcome arrived.
    bool receives(const std::shared_ptr<Outcome>& outcome)
    {
        // clang-format off
        return pumpUntil([&] { return !outcome->body.empty(); });
        // clang-format on
    }

    // Pumps for a while, so whatever would still arrive has the chance to.
    void idle(std::chrono::milliseconds length)
    {
        // clang-format off
        pumpUntil([] { return false; }, length);
        // clang-format on
    }

    Request get(const std::string& path) const
    {
        Request request;
        request.url = url(path);
        return request;
    }

    ServerState state;
    Poco::UInt16 port = 0;
    std::unique_ptr<Poco::Net::HTTPServer> server;
    varn::runtime::Runtime runtime{{"http_client_test"}};
    std::unique_ptr<HttpClient> client = std::make_unique<HttpClient>(runtime);
};

} // namespace

TEST_F(HttpClientTest, DeliversTheHeadBeforeTheBodyAndTheBodyAsItArrives)
{
    auto outcome = std::make_shared<Outcome>();

    // clang-format off
    const auto release = [this](std::string_view)
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.releaseSecond = true;
        state.released.notify_all();
    };
    // clang-format on

    send(get("/paced"), outcome, release);
    ASSERT_TRUE(settles(outcome));

    EXPECT_FALSE(outcome->error.has_value());
    ASSERT_GE(outcome->events.size(), 4u);
    EXPECT_EQ(outcome->events.front(), "head");
    EXPECT_EQ(outcome->events.back(), "complete");
    EXPECT_EQ(outcome->head->status, 200);
    EXPECT_EQ(outcome->head->header("x-PACED").value_or(""), "yes");
    EXPECT_FALSE(outcome->head->contentLength.has_value());
    EXPECT_EQ(outcome->body, "firstsecond");
    ASSERT_FALSE(outcome->progress.empty());
    EXPECT_EQ(outcome->progress.back().received, 11u);
    EXPECT_FALSE(outcome->progress.back().total.has_value());
}

TEST_F(HttpClientTest, ReportsTheTotalOfABodyWithALength)
{
    auto outcome = std::make_shared<Outcome>();
    send(get("/hello"), outcome);
    ASSERT_TRUE(settles(outcome));

    EXPECT_FALSE(outcome->error.has_value());
    EXPECT_EQ(outcome->body, "hello");
    EXPECT_EQ(outcome->head->contentLength.value_or(0), 5u);
    EXPECT_EQ(outcome->head->url, url("/hello"));
    ASSERT_FALSE(outcome->progress.empty());
    EXPECT_EQ(outcome->progress.back().total.value_or(0), 5u);
}

TEST_F(HttpClientTest, CancelsFromAnotherThread)
{
    auto outcome = std::make_shared<Outcome>();
    Handle handle = send(get("/endless"), outcome);
    ASSERT_TRUE(receives(outcome));

    const auto started = std::chrono::steady_clock::now();
    // clang-format off
    std::thread canceller([handle] { handle.cancel(); });
    // clang-format on
    canceller.join();
    EXPECT_FALSE(handle.active());

    ASSERT_TRUE(settles(outcome));
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(2));
    ASSERT_TRUE(outcome->error.has_value());
    EXPECT_EQ(outcome->error->code, ErrorCode::Cancelled);
    EXPECT_EQ(outcome->events.back(), "complete");

    // Nothing arrives once the completion did.
    const std::size_t seen = outcome->events.size();
    idle(std::chrono::milliseconds(200));
    EXPECT_EQ(outcome->events.size(), seen);
}

TEST_F(HttpClientTest, EndsARequestAtItsDeadline)
{
    auto outcome = std::make_shared<Outcome>();
    Request request = get("/slow");
    request.timeout = std::chrono::milliseconds(300);

    const auto started = std::chrono::steady_clock::now();
    send(std::move(request), outcome);
    ASSERT_TRUE(settles(outcome));

    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(1200));
    ASSERT_TRUE(outcome->error.has_value());
    EXPECT_EQ(outcome->error->code, ErrorCode::Timeout);
    EXPECT_NE(outcome->error->message.find("did not finish within its timeout"), std::string::npos);
}

TEST_F(HttpClientTest, RefusesADeclaredLengthOverTheLimitBeforeReadingIt)
{
    auto outcome = std::make_shared<Outcome>();
    Request request = get("/large");
    request.maxResponseBytes = 1024 * 1024;

    send(std::move(request), outcome);
    ASSERT_TRUE(settles(outcome));

    ASSERT_TRUE(outcome->error.has_value());
    EXPECT_EQ(outcome->error->code, ErrorCode::TooLarge);
    EXPECT_NE(outcome->error->message.find("1048576"), std::string::npos);
    EXPECT_NE(outcome->error->message.find(url("/large")), std::string::npos);
    EXPECT_TRUE(outcome->body.empty());
}

TEST_F(HttpClientTest, StopsAStreamedBodyAtTheLimit)
{
    auto outcome = std::make_shared<Outcome>();
    Request request = get("/endless");
    request.maxResponseBytes = 1024 * 1024;

    send(std::move(request), outcome);
    ASSERT_TRUE(settles(outcome));

    ASSERT_TRUE(outcome->error.has_value());
    EXPECT_EQ(outcome->error->code, ErrorCode::TooLarge);
    EXPECT_LE(outcome->body.size(), 1024u * 1024u);
}

TEST_F(HttpClientTest, HoldsOnlyABoundedWindowWhileTheLoopIsNotPolled)
{
    auto outcome = std::make_shared<Outcome>();
    Request request = get("/endless");
    request.maxResponseBytes = 1024ull * 1024ull * 1024ull;
    Handle handle = send(std::move(request), outcome);

    // A host that stops polling stops the download, rather than having the whole body gather in memory.
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    EXPECT_LT(state.endlessWritten.load(), 16ull * 1024ull * 1024ull);

    handle.cancel();
    ASSERT_TRUE(settles(outcome));
    EXPECT_EQ(outcome->error->code, ErrorCode::Cancelled) << outcome->error->message;
}

TEST_F(HttpClientTest, ResumesADownloadWithARange)
{
    auto whole = std::make_shared<Outcome>();
    send(get("/range"), whole);
    ASSERT_TRUE(settles(whole));
    ASSERT_EQ(whole->body, RangeBody::text());
    EXPECT_FALSE(whole->head->contentRange().has_value());

    auto rest = std::make_shared<Outcome>();
    Request request = get("/range");
    request.headers.emplace_back("Range", "bytes=600-");
    send(std::move(request), rest);
    ASSERT_TRUE(settles(rest));

    EXPECT_FALSE(rest->error.has_value());
    EXPECT_EQ(rest->head->status, 206);
    const std::optional<ContentRange> range = rest->head->contentRange();
    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->first, 600u);
    EXPECT_EQ(range->last, 999u);
    EXPECT_EQ(range->completeLength.value_or(0), 1000u);
    EXPECT_EQ(whole->body.substr(0, 600) + rest->body, RangeBody::text());
}

TEST_F(HttpClientTest, ReadsAContentRangeOnlyInItsValidForms)
{
    ResponseHead head;
    head.headers = {{"Content-Range", "bytes 0-9/*"}};
    ASSERT_TRUE(head.contentRange().has_value());
    EXPECT_FALSE(head.contentRange()->completeLength.has_value());

    for (const char* invalid : {"bytes */100", "bytes 9-0/100", "bytes 0-99/50", "items 0-1/2", "bytes 0-1/x", "bytes -1-2/3"})
    {
        head.headers = {{"Content-Range", invalid}};
        EXPECT_FALSE(head.contentRange().has_value()) << invalid;
    }
}

TEST_F(HttpClientTest, AnswersNotModifiedToAConditionalRequest)
{
    auto first = std::make_shared<Outcome>();
    send(get("/cached"), first);
    ASSERT_TRUE(settles(first));
    ASSERT_EQ(first->head->header("etag").value_or(""), "\"v1\"");

    auto second = std::make_shared<Outcome>();
    Request request = get("/cached");
    request.headers.emplace_back("If-None-Match", std::string(*first->head->header("etag")));
    send(std::move(request), second);
    ASSERT_TRUE(settles(second));

    EXPECT_FALSE(second->error.has_value());
    EXPECT_EQ(second->head->status, 304);
    EXPECT_EQ(second->head->contentLength.value_or(1), 0u);
    EXPECT_TRUE(second->body.empty());
}

TEST_F(HttpClientTest, ReusesOneConnectionForSequentialRequests)
{
    for (int index = 0; index < 5; ++index)
    {
        auto outcome = std::make_shared<Outcome>();
        send(get(index % 2 == 0 ? "/hello" : "/range"), outcome);
        ASSERT_TRUE(settles(outcome));
        ASSERT_FALSE(outcome->error.has_value());
    }

    EXPECT_EQ(server->totalConnections(), 1);
}

TEST_F(HttpClientTest, OpensANewConnectionWhenTheIdleOneWasClosed)
{
    auto first = std::make_shared<Outcome>();
    send(get("/hello"), first);
    ASSERT_TRUE(settles(first));

    // A server that closes its idle connections leaves the pooled one dead, which the next request must not fail on.
    server->stopAll(true);
    Poco::ThreadPool::defaultPool().joinAll();
    Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("127.0.0.1", port));
    auto* params = new Poco::Net::HTTPServerParams;
    params->setKeepAlive(true);
    server = std::make_unique<Poco::Net::HTTPServer>(new TestHandlerFactory(state), socket, params);
    server->start();

    auto second = std::make_shared<Outcome>();
    send(get("/hello"), second);
    ASSERT_TRUE(settles(second));
    EXPECT_FALSE(second->error.has_value());
    EXPECT_EQ(second->body, "hello");
}

TEST_F(HttpClientTest, FollowsARedirectAndReportsWhereItEnded)
{
    auto outcome = std::make_shared<Outcome>();
    send(get("/moved"), outcome);
    ASSERT_TRUE(settles(outcome));

    EXPECT_FALSE(outcome->error.has_value());
    EXPECT_EQ(outcome->head->status, 200);
    EXPECT_EQ(outcome->head->url, url("/hello"));
    EXPECT_EQ(outcome->body, "hello");

    auto refused = std::make_shared<Outcome>();
    Request request = get("/moved");
    request.redirects = RedirectPolicy::Error;
    send(std::move(request), refused);
    ASSERT_TRUE(settles(refused));
    ASSERT_TRUE(refused->error.has_value());
    EXPECT_EQ(refused->error->code, ErrorCode::Redirect);
}

TEST_F(HttpClientTest, StreamsARequestBodyFromACallback)
{
    auto outcome = std::make_shared<Outcome>();
    Request request = get("/echo");
    request.method = "POST";
    auto pieces = std::make_shared<int>(0);

    // clang-format off
    request.bodySource = [pieces](char* buffer, std::size_t capacity) -> std::size_t
    {
        if (*pieces == 3)
        {
            return 0;
        }

        const std::size_t length = std::min<std::size_t>(capacity, 1000);
        std::fill(buffer, buffer + length, static_cast<char>('0' + *pieces));
        ++*pieces;
        return length;
    };
    // clang-format on

    send(std::move(request), outcome);
    ASSERT_TRUE(settles(outcome));

    EXPECT_FALSE(outcome->error.has_value());
    EXPECT_EQ(outcome->body, std::string(1000, '0') + std::string(1000, '1') + std::string(1000, '2'));
    std::lock_guard<std::mutex> lock(state.mutex);
    EXPECT_EQ(state.transferEncoding, "chunked");
}

TEST_F(HttpClientTest, RefusesABodySourceThatBreaksItsLength)
{
    auto outcome = std::make_shared<Outcome>();
    Request request = get("/echo");
    request.method = "PUT";
    request.bodyLength = 10;

    // clang-format off
    request.bodySource = [](char* buffer, std::size_t capacity) -> std::size_t
    {
        std::fill(buffer, buffer + std::min<std::size_t>(capacity, 20), 'z');
        return std::min<std::size_t>(capacity, 20);
    };
    // clang-format on

    send(std::move(request), outcome);
    ASSERT_TRUE(settles(outcome));
    ASSERT_TRUE(outcome->error.has_value());
    EXPECT_EQ(outcome->error->code, ErrorCode::Invalid);
}

TEST_F(HttpClientTest, EndsTheTransferWhenACallbackThrows)
{
    auto outcome = std::make_shared<Outcome>();

    // clang-format off
    send(get("/endless"), outcome, [](std::string_view) { throw std::runtime_error("Deliberate chunk failure."); });
    // clang-format on

    ASSERT_TRUE(settles(outcome));
    ASSERT_TRUE(outcome->error.has_value());
    EXPECT_EQ(outcome->error->code, ErrorCode::Callback);
    EXPECT_NE(outcome->error->message.find("Deliberate chunk failure."), std::string::npos);
    EXPECT_EQ(std::count(outcome->events.begin(), outcome->events.end(), "chunk"), 1);
}

TEST_F(HttpClientTest, ReportsTypedErrors)
{
    struct Case
    {
        std::string url;
        std::string method;
        ErrorCode code;
    };

    Poco::Net::ServerSocket closed(Poco::Net::SocketAddress("127.0.0.1", 0));
    const std::string refusedUrl = "http://127.0.0.1:" + std::to_string(closed.address().port()) + "/";
    closed.close();

    std::vector<Case> cases = {
        {"ftp://127.0.0.1/file", "GET", ErrorCode::Invalid},
        {url("/hello"), "BAD METHOD", ErrorCode::Invalid},
        {refusedUrl, "GET", ErrorCode::Network},
    };

    for (const Case& wanted : cases)
    {
        auto outcome = std::make_shared<Outcome>();
        Request request;
        request.url = wanted.url;
        request.method = wanted.method;
        send(std::move(request), outcome);
        ASSERT_TRUE(settles(outcome))
            << wanted.url;
        ASSERT_TRUE(outcome->error.has_value()) << wanted.url;
        EXPECT_EQ(outcome->error->code, wanted.code) << wanted.url << ": " << outcome->error->message;
        EXPECT_EQ(outcome->events, std::vector<std::string>{"complete"});
    }
}

#if defined(VARN_ENABLE_TLS)
TEST_F(HttpClientTest, ReportsARefusedHandshakeAsTls)
{
    // A server that answers the hello of the client with a fatal alert of the handshake refuses it, which every transport reads the same way.
    Poco::Net::ServerSocket refusing(Poco::Net::SocketAddress("127.0.0.1", 0));
    std::atomic<bool> done{false};

    // clang-format off
    std::thread responder([&refusing, &done]
    {
        const unsigned char alert[] = {0x15, 0x03, 0x03, 0x00, 0x02, 0x02, 0x28};
        std::vector<Poco::Net::StreamSocket> peers;
        while (!done.load())
        {
            if (!refusing.poll(Poco::Timespan(0, 50000), Poco::Net::Socket::SELECT_READ))
            {
                continue;
            }

            peers.push_back(refusing.acceptConnection());
            char hello[4096];
            peers.back().setReceiveTimeout(Poco::Timespan(2, 0));
            peers.back().receiveBytes(hello, sizeof(hello));
            peers.back().sendBytes(alert, sizeof(alert));
        }
    });
    // clang-format on

    auto outcome = std::make_shared<Outcome>();
    Request request;
    request.url = "https://127.0.0.1:" + std::to_string(refusing.address().port()) + "/";
    send(std::move(request), outcome);
    const bool settled = settles(outcome);
    done.store(true);
    responder.join();

    ASSERT_TRUE(settled);
    ASSERT_TRUE(outcome->error.has_value());
    EXPECT_EQ(outcome->error->code, ErrorCode::Tls) << outcome->error->message;
}
#endif

TEST_F(HttpClientTest, RefusesARequestItCannotSendBeforeAnyTransportSeesIt)
{
    std::vector<Request> refused(5, get("/hello"));
    refused[0].timeout = std::chrono::milliseconds(0);
    refused[1].timeout = std::chrono::hours(24 * 365 * 1000);
    refused[2].headers.emplace_back("X-Forged", "value\r\nX-Other: injected");
    refused[3].body = "held";
    refused[3].bodySource = [](char*, std::size_t) -> std::size_t
    { return 0; };
    refused[4].bodyLength = 4;

    for (Request& request : refused)
    {
        auto outcome = std::make_shared<Outcome>();
        send(std::move(request), outcome);
        ASSERT_TRUE(settles(outcome));
        ASSERT_TRUE(outcome->error.has_value());
        EXPECT_EQ(outcome->error->code, ErrorCode::Invalid) << outcome->error->message;
    }

    EXPECT_EQ(server->totalConnections(), 0);
}

TEST_F(HttpClientTest, DestroyingTheClientEndsItsRequestsWithoutCallingThem)
{
    auto outcome = std::make_shared<Outcome>();
    send(get("/endless"), outcome);
    ASSERT_TRUE(receives(outcome));

    client.reset();
    idle(std::chrono::milliseconds(200));
    EXPECT_FALSE(outcome->completed);
}

// A completion already on its way to the loop when the client goes away is dropped as well, such as the refusal of a request the loop never ran.
TEST_F(HttpClientTest, DestroyingTheClientDropsACompletionAlreadyOnItsWay)
{
    auto outcome = std::make_shared<Outcome>();
    Request request = get("/hello");
    request.method = "BAD METHOD";
    send(std::move(request), outcome);

    client.reset();
    idle(std::chrono::milliseconds(100));
    EXPECT_FALSE(outcome->completed);
}

} // namespace varn::http::client
