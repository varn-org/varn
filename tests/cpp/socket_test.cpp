#include "drivers/poco/PocoSocketText.h"

#include "varn/runtime/Runtime.h"
#include "varn/socket/SocketTransport.h"
#include "varn/varn.h"

#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketDefs.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace varn::socket
{

namespace
{

class SocketTestHelpers
{
public:
    // Runs a chunk on a fresh runtime until its loop is idle and answers its exit code, which an assertion of the chunk turns into a failure.
    static int run(const char* chunk)
    {
        varn_runtime* runtime = varn_runtime_new();
        const int code = varn_runtime_run_string(runtime, chunk, "=socket");
        varn_runtime_free(runtime);
        return code;
    }

    // Polls the runtime until the flag is set or a deadline passes.
    static void pollUntil(varn::runtime::Runtime& runtime, const bool& done)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!done && std::chrono::steady_clock::now() < deadline)
        {
            std::ignore = runtime.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

TEST(SocketText, NamesAnIpv6TargetInBrackets)
{
    EXPECT_EQ(PocoSocketText::target("127.0.0.1", 80), "127.0.0.1:80");
    EXPECT_EQ(PocoSocketText::target("::1", 80), "[::1]:80");
    EXPECT_EQ(PocoSocketText::describe(SocketEndpoint{"/tmp/varn.sock", std::nullopt}), "/tmp/varn.sock");
}

TEST(SocketText, ReportsABusyAddressByItself)
{
    const Poco::Net::NetException busy("Address already in use", "127.0.0.1:41000", POCO_EADDRINUSE);
    EXPECT_EQ(PocoSocketText::bindFailure("127.0.0.1:41000", busy), "The address 127.0.0.1:41000 is already in use.");

    const Poco::Net::NetException denied("Permission denied", POCO_EACCES);
    const std::string text = PocoSocketText::bindFailure("0.0.0.0:80", denied);
    EXPECT_EQ(text.rfind("The address 0.0.0.0:80 could not be bound: Net Exception: Permission denied (system error ", 0), 0u) << text;
}

TEST(SocketText, NamesTheAddressOfAFailedConnect)
{
    EXPECT_EQ(PocoSocketText::connectFailure("127.0.0.1:9", POCO_ECONNREFUSED), "The connection to 127.0.0.1:9 was refused.");
    EXPECT_EQ(PocoSocketText::connectFailure("127.0.0.1:9", POCO_ETIMEDOUT), "The connection to 127.0.0.1:9 timed out.");
    EXPECT_EQ(PocoSocketText::connectFailure("127.0.0.1:9", Poco::Net::ConnectionRefusedException("127.0.0.1:9", POCO_ECONNREFUSED)), "The connection to 127.0.0.1:9 was refused.");

    const std::string unreachable = PocoSocketText::connectFailure("10.0.0.1:9", POCO_ENETUNREACH);
    EXPECT_EQ(unreachable.rfind("The connection to 10.0.0.1:9 failed: ", 0), 0u) << unreachable;
    EXPECT_NE(unreachable.find("(system error " + std::to_string(POCO_ENETUNREACH) + ")."), std::string::npos) << unreachable;
}

TEST(SocketText, ContinuesASubjectWithTheDisplayTextAndTheSystemError)
{
    EXPECT_EQ(PocoSocketText::failure("The send to 127.0.0.1:5000 failed", Poco::Net::ConnectionResetException(POCO_ECONNRESET)), "The send to 127.0.0.1:5000 failed: Connection reset by peer (system error " + std::to_string(POCO_ECONNRESET) + ").");
    EXPECT_EQ(PocoSocketText::failure("The receive failed", std::runtime_error("Broken.\n")), "The receive failed: Broken.");
    EXPECT_EQ(PocoSocketText::resolveFailure("host.invalid", Poco::Net::HostNotFoundException("host.invalid")), "The host \"host.invalid\" could not be resolved.");
}

// A listener on port 0 reports the port the system chose, and a second one on that port fails with its address instead of sharing it.
TEST(SocketTransport, AListenerOnABusyPortFailsWithItsAddress)
{
    varn::runtime::Runtime runtime(std::vector<std::string>{"varn"});

    std::shared_ptr<TcpListener> first;
    // clang-format off
    SocketTransport::listenAsync(runtime, "127.0.0.1", 0, TcpListenOptions{16, false, 1024}, [&first](std::shared_ptr<TcpListener> listener, const std::string& error)
    {
        EXPECT_EQ(error, "");
        first = std::move(listener);
    });
    // clang-format on
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(first->endpoint().port.has_value());
    const int port = *first->endpoint().port;
    EXPECT_GT(port, 0);
    EXPECT_EQ(first->endpoint().host, "127.0.0.1");

    std::string busy;
    // clang-format off
    SocketTransport::listenAsync(runtime, "127.0.0.1", port, TcpListenOptions{16, false, 1024}, [&busy](std::shared_ptr<TcpListener> listener, const std::string& error)
    {
        EXPECT_EQ(listener, nullptr);
        busy = error;
    });
    // clang-format on
    EXPECT_EQ(busy, "The address 127.0.0.1:" + std::to_string(port) + " is already in use.");

    first->close();
}

// A name is looked up on the I/O pool and answered on the loop, while a numeric address answers at once.
TEST(SocketTransport, ResolvesANameOffTheLoopThread)
{
    varn::runtime::Runtime runtime(std::vector<std::string>{"varn"});

    bool numericDone = false;
    // clang-format off
    SocketTransport::resolveAsync(runtime, "::1", [&numericDone](bool ok, std::vector<std::string> addresses, const std::string&)
    {
        EXPECT_TRUE(ok);
        EXPECT_EQ(addresses, std::vector<std::string>{"::1"});
        numericDone = true;
    });
    // clang-format on
    EXPECT_TRUE(numericDone);

    bool done = false;
    const auto caller = std::this_thread::get_id();
    std::string error;
    // clang-format off
    SocketTransport::resolveAsync(runtime, "host.invalid", [&done, &error, caller](bool ok, std::vector<std::string>, const std::string& reason)
    {
        EXPECT_FALSE(ok);
        EXPECT_EQ(std::this_thread::get_id(), caller);
        error = reason;
        done = true;
    });
    // clang-format on
    EXPECT_FALSE(done);

    SocketTestHelpers::pollUntil(runtime, done);
    EXPECT_TRUE(done);
    EXPECT_EQ(error, "The host \"host.invalid\" could not be resolved.");
}

TEST(SocketModule, BindsPortZeroAndRefusesABusyPortByItsAddress)
{
    const char* chunk =
        "local async = require('async')\n"
        "local socket = require('socket')\n"
        "async.run(function()\n"
        "  local listener = socket.tcp.listen('127.0.0.1', 0):await()\n"
        "  assert(listener.port > 0)\n"
        "  local _, err = socket.tcp.listen('127.0.0.1', listener.port):await()\n"
        "  assert(err == 'The address 127.0.0.1:' .. listener.port .. ' is already in use.', err)\n"
        "  local udp = socket.udp.bind('127.0.0.1', 0):await()\n"
        "  local _, udpErr = socket.udp.bind('127.0.0.1', udp.port):await()\n"
        "  assert(udpErr == 'The address 127.0.0.1:' .. udp.port .. ' is already in use.', udpErr)\n"
        "  listener:close():await()\n"
        "  udp:close():await()\n"
        "end)\n";
    EXPECT_EQ(SocketTestHelpers::run(chunk), 0);
}

TEST(SocketModule, AClosedSocketRejectsWhatWasPendingWithTheClose)
{
    const char* chunk =
        "local async = require('async')\n"
        "local socket = require('socket')\n"
        "async.run(function()\n"
        "  local listener = socket.tcp.listen('127.0.0.1', 0):await()\n"
        "  local accept = listener:accept()\n"
        "  local conn = socket.tcp.connect('127.0.0.1', listener.port):await()\n"
        "  local peer = accept:await()\n"
        "  local nextAccept = listener:accept()\n"
        "  local receive = conn:receive()\n"
        "  async.sleep(5):await()\n"
        "  conn:close():await()\n"
        "  listener:close():await()\n"
        "  assert(select(2, receive:await()) == 'The socket was closed.')\n"
        "  assert(select(2, nextAccept:await()) == 'The listener was closed.')\n"
        "  assert(peer:receive():await() == '')\n"
        "  peer:close():await()\n"
        "end)\n";
    EXPECT_EQ(SocketTestHelpers::run(chunk), 0);
}

TEST(SocketModule, SetsOptionsReportsAddressesAndBoundsQueuedSends)
{
    const char* chunk =
        "local async = require('async')\n"
        "local socket = require('socket')\n"
        "async.run(function()\n"
        "  local listener = socket.tcp.listen('127.0.0.1', 0):await()\n"
        "  local conn = socket.tcp.connect('127.0.0.1', listener.port, { maxQueuedBytes = 10 }):await()\n"
        "  local peer = listener:accept():await()\n"
        "  assert(conn.peerPort == listener.port and peer.peerPort == conn.localPort)\n"
        "  assert(conn:setNoDelay(true):await() == 'ok' and conn:setKeepAlive(true):await() == 'ok')\n"
        "  local first = conn:send('12345678')\n"
        "  assert(conn.pendingBytes == 8)\n"
        "  local _, refused = conn:send('123'):await()\n"
        "  assert(refused and refused:find('limit of 10 bytes', 1, true), refused)\n"
        "  assert(first:await() == 'ok' and conn.pendingBytes == 0)\n"
        "  assert(conn:shutdown('send'):await() == 'ok')\n"
        "  assert(peer:receive():await() == '12345678')\n"
        "  assert(peer:receive():await() == '')\n"
        "  conn:close():await()\n"
        "  peer:close():await()\n"
        "  listener:close():await()\n"
        "end)\n";
    EXPECT_EQ(SocketTestHelpers::run(chunk), 0);
}

TEST(SocketModule, ConnectsUdpSocketsAndResolvesNames)
{
    const char* chunk =
        "local async = require('async')\n"
        "local socket = require('socket')\n"
        "async.run(function()\n"
        "  assert(#socket.resolve('localhost'):await() >= 1)\n"
        "  local server = socket.udp.bind('localhost', 0):await()\n"
        "  local client = socket.udp.bind('localhost', 0):await()\n"
        "  assert(client:connect('localhost', server.port):await() == 'ok')\n"
        "  assert(client:send('ping'):await() == 'ok')\n"
        "  assert(server:recvFrom():await().data == 'ping')\n"
        "  assert(client:setBroadcast(true):await() == 'ok')\n"
        "  server:close():await()\n"
        "  client:close():await()\n"
        "end)\n";
    EXPECT_EQ(SocketTestHelpers::run(chunk), 0);
}

// A stop from another thread lands while names are still being looked up on the I/O pool, and the teardown must neither hang nor touch a socket the lookup finished for.
TEST(SocketModule, AStopDuringNameLookupsTearsDownCleanly)
{
    const char* chunk =
        "local async = require('async')\n"
        "local socket = require('socket')\n"
        "async.spawn(function()\n"
        "  local listener = socket.tcp.listen('localhost', 0):await()\n"
        "  local udp = socket.udp.bind('localhost', 0):await()\n"
        "  for _ = 1, 8 do\n"
        "    async.spawn(function()\n"
        "      while true do\n"
        "        local conn = socket.tcp.connect('localhost', listener.port):await()\n"
        "        if conn then conn:close():await() end\n"
        "        udp:sendTo('localhost', udp.port, 'x'):await()\n"
        "      end\n"
        "    end)\n"
        "  end\n"
        "end)\n";

    for (int attempt = 0; attempt < 10; ++attempt)
    {
        varn_runtime* runtime = varn_runtime_new();
        ASSERT_NE(runtime, nullptr);

        // clang-format off
        std::thread runner([runtime, chunk]
        {
            std::ignore = varn_runtime_run_string(runtime, chunk, "=lookups");
        });
        // clang-format on

        std::this_thread::sleep_for(std::chrono::milliseconds(20 + attempt * 5));
        varn_runtime_stop(runtime);
        runner.join();
        varn_runtime_free(runtime);
    }
}

} // namespace

} // namespace varn::socket
