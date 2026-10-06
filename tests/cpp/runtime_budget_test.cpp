#include "varn/runtime/EventLoop.h"
#include "varn/runtime/WorkLedger.h"
#include "varn/varn.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace varn::runtime
{

namespace
{

using Clock = std::chrono::steady_clock;

constexpr long long kFrameBudgetNanoseconds = 4'000'000;

// A poll overruns its budget by at most one job and one pass over the sockets, which a sanitizer or a step of the collector can stretch, yet stays far below a poll without a bound.
// A sanitized build runs several times slower, so it keeps the same shapes with a lighter load and a looser bound, and checks only that a budget helps rather than by how much.
#if defined(VARN_TESTS_SANITIZED)
constexpr long long kBoundMilliseconds = 500;
constexpr int kGain = 1;
constexpr int kBoundedSleeperCount = 3000;
#else
constexpr long long kBoundMilliseconds = 100;
constexpr int kGain = 10;
constexpr int kBoundedSleeperCount = 30000;
#endif

class BudgetTestHelpers
{
public:
    // Answers the script of a number of tasks that each sleep one frame in a loop, which together can take longer to wake than the frame they sleep.
    static std::string sleepers(int count)
    {
        return "local async = require('async')\nwakes = 0\nfor _ = 1, " + std::to_string(count) + " do\n  async.spawn(function() while true do async.sleep(16):await() wakes = wakes + 1 end end)\nend\n";
    }

    static inline long long reported = 0;

    // Keeps the last number a script handed to `host.report`, which is how a script tells the test how far it got.
    static const char* report(const char* argument, void*)
    {
        reported = std::stoll(argument != nullptr ? argument : "0");
        return "null";
    }

    static varn_runtime* start(const char* chunk)
    {
        reported = 0;
        varn_runtime* runtime = varn_runtime_new();
        if (runtime == nullptr)
        {
            return nullptr;
        }

        varn_runtime_register(runtime, "report", &BudgetTestHelpers::report, nullptr);
        if (varn_runtime_load_string(runtime, chunk, "=budget") != 0)
        {
            varn_runtime_free(runtime);
            return nullptr;
        }

        return runtime;
    }

    // Polls the way a host that renders at sixty frames per second does, once per frame of sixteen milliseconds, with or without a budget.
    static long long frame(varn_runtime* runtime, bool budgeted)
    {
        const auto started = Clock::now();
        if (budgeted)
        {
            varn_runtime_poll_budget(runtime, kFrameBudgetNanoseconds, nullptr);
        }
        else
        {
            varn_runtime_poll(runtime);
        }

        const auto spent = Clock::now() - started;
        std::this_thread::sleep_until(started + std::chrono::milliseconds(16));
        return std::chrono::duration_cast<std::chrono::milliseconds>(spent).count();
    }

    // Counts the frames a script needs until it reports the target, giving up after the limit.
    static int framesUntil(varn_runtime* runtime, bool budgeted, long long target, int limit)
    {
        int frames = 0;
        while (reported < target && frames < limit)
        {
            frame(runtime, budgeted);
            ++frames;
        }

        return frames;
    }

    // Runs a few frames so a script reaches the point where it starts the work being measured.
    static void warm(varn_runtime* runtime, bool budgeted)
    {
        for (int frame = 0; frame < 10; ++frame)
        {
            BudgetTestHelpers::frame(runtime, budgeted);
        }
    }

    // Measures the bytes per second a stream moves over thirty frames, polled with or without a budget.
    static double streamRate(const char* chunk, bool budgeted)
    {
        varn_runtime* runtime = start(chunk);
        if (runtime == nullptr)
        {
            return 0.0;
        }

        warm(runtime, budgeted);
        const long long before = reported;
        const auto started = Clock::now();
        for (int frame = 0; frame < 30; ++frame)
        {
            BudgetTestHelpers::frame(runtime, budgeted);
        }

        const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
        varn_runtime_free(runtime);
        return static_cast<double>(reported - before) / seconds / 1048576.0;
    }

    static void spin(std::chrono::milliseconds duration)
    {
        const auto until = Clock::now() + duration;
        while (Clock::now() < until)
        {
        }
    }
};


// A server of the runtime answers a client of the same runtime that sends one request at a time over one connection.
const char* const kSequentialRequests =
    "local http = require('http')\n"
    "local socket = require('socket')\n"
    "local async = require('async')\n"
    "local app = http.createApp()\n"
    "app:get('/ping', function(ctx) ctx:text('pong') end)\n"
    "local port = app:listen({ host = '127.0.0.1', port = 0 }).port\n"
    "async.spawn(function()\n"
    "  async.sleep(50):await()\n"
    "  local conn = socket.tcp.connect('127.0.0.1', port):await()\n"
    "  for i = 1, 20 do\n"
    "    conn:send('GET /ping HTTP/1.1\\r\\nHost: local\\r\\n\\r\\n'):await()\n"
    "    local seen = ''\n"
    "    while not seen:find('pong', 1, true) do seen = seen .. conn:receive(65536):await() end\n"
    "    host.report(i)\n"
    "  end\n"
    "end)\n";

// A sender that writes as fast as each send resolves and a receiver of the same runtime that reports every byte it read.
const char* const kStream =
    "local socket = require('socket')\n"
    "local async = require('async')\n"
    "local listener = socket.tcp.listen('127.0.0.1', 0):await()\n"
    "async.spawn(function()\n"
    "  local conn = listener:accept():await()\n"
    "  local total = 0\n"
    "  while true do\n"
    "    local data = conn:receive(65536):await()\n"
    "    if not data or #data == 0 then break end\n"
    "    total = total + #data\n"
    "    host.report(total)\n"
    "  end\n"
    "end)\n"
    "async.spawn(function()\n"
    "  local conn = socket.tcp.connect('127.0.0.1', listener.port):await()\n"
    "  local block = string.rep('x', 65536)\n"
    "  while true do conn:send(block):await() end\n"
    "end)\n";

// A client of the same runtime uploads a body of a few mebibytes, one request after the other, to a server that answers its size.
const char* const kUploads =
    "local http = require('http')\n"
    "local socket = require('socket')\n"
    "local async = require('async')\n"
    "local app = http.createApp()\n"
    "app:post('/upload', function(ctx) ctx:text('size=' .. #ctx.req.body .. ';') end)\n"
    "local port = app:listen({ host = '127.0.0.1', port = 0 }).port\n"
    "async.spawn(function()\n"
    "  async.sleep(50):await()\n"
    "  local conn = socket.tcp.connect('127.0.0.1', port):await()\n"
    "  local body = string.rep('u', 4 * 1048576)\n"
    "  for i = 1, 3 do\n"
    "    conn:send('POST /upload HTTP/1.1\\r\\nHost: local\\r\\nContent-Length: ' .. #body .. '\\r\\n\\r\\n' .. body):await()\n"
    "    local seen = ''\n"
    "    while not seen:find(';', 1, true) do seen = seen .. conn:receive(65536):await() end\n"
    "    assert(seen:find('size=' .. #body, 1, true), 'The server read a partial body.')\n"
    "    host.report(i)\n"
    "  end\n"
    "end)\n";

// A burst of clients connects at once to a listener whose task accepts one connection after the other.
const char* const kAcceptBurst =
    "local socket = require('socket')\n"
    "local async = require('async')\n"
    "local listener = socket.tcp.listen('127.0.0.1', 0, { backlog = 256 }):await()\n"
    "local clients = {}\n"
    "async.spawn(function()\n"
    "  for i = 1, 100 do clients[i] = listener:accept():await() host.report(i) end\n"
    "end)\n"
    "async.spawn(function()\n"
    "  async.sleep(20):await()\n"
    "  for i = 1, 100 do async.spawn(function() clients[100 + i] = socket.tcp.connect('127.0.0.1', listener.port):await() end) end\n"
    "end)\n";

} // namespace

// A pass runs only the work that was ready when it started, so work it arms runs in a later pass of the same poll while the budget lasts.
TEST(RuntimeBudget, PassesRunWhatWasReadyAndRepeatWhileTheyProgress)
{
    auto ledger = std::make_shared<WorkLedger>();
    EventLoop loop(ledger);

    std::vector<std::string> order;
    // clang-format off
    loop.post([&]
    {
        order.push_back("first");
        loop.post([&] { order.push_back("posted"); });
        loop.postDelayed(0, [&] { order.push_back("timer"); });
    });
    loop.post([&] { order.push_back("second"); });
    // clang-format on

    const EventLoop::PollResult result = loop.poll(std::chrono::seconds(5));

    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], "first");
    EXPECT_EQ(order[1], "second");
    EXPECT_EQ(order[2], "posted");
    EXPECT_EQ(order[3], "timer");
    EXPECT_TRUE(result.ran);
    EXPECT_FALSE(result.pending);
    EXPECT_EQ(result.idleMilliseconds, -1);
    loop.shutdownIo();
}

// The budget is checked between jobs, so a poll stops near it and leaves the rest of the pass queued in order for the next poll.
TEST(RuntimeBudget, StopsBetweenJobsOnceTheBudgetIsSpent)
{
    auto ledger = std::make_shared<WorkLedger>();
    EventLoop loop(ledger);

    std::vector<int> ran;
    for (int index = 0; index < 50; ++index)
    {
        // clang-format off
        loop.post([&ran, index]
        {
            BudgetTestHelpers::spin(std::chrono::milliseconds(1));
            ran.push_back(index);
        });
        // clang-format on
    }

    const EventLoop::PollResult first = loop.poll(std::chrono::milliseconds(5));
    EXPECT_TRUE(first.ran);
    EXPECT_TRUE(first.pending);
    EXPECT_EQ(first.idleMilliseconds, 0);
    EXPECT_GE(ran.size(), 1u);
    EXPECT_LT(ran.size(), 50u);

    const std::size_t afterFirst = ran.size();
    std::ignore = loop.poll(std::chrono::nanoseconds::zero());
    ASSERT_EQ(ran.size(), afterFirst + 1);

    std::ignore = loop.poll(std::chrono::seconds(5));
    ASSERT_EQ(ran.size(), 50u);
    for (int index = 0; index < 50; ++index)
    {
        EXPECT_EQ(ran[static_cast<std::size_t>(index)], index);
    }

    loop.shutdownIo();
}

// A poll with nothing to do runs nothing and tells the host it may sleep until something arrives.
TEST(RuntimeBudget, AnIdlePollRunsNothing)
{
    varn_runtime* runtime = varn_runtime_new();
    ASSERT_NE(runtime, nullptr);

    varn_poll_result result{1, 1, 0};
    EXPECT_EQ(varn_runtime_poll_budget(runtime, kFrameBudgetNanoseconds, &result), 0);
    EXPECT_EQ(result.ran, 0);
    EXPECT_EQ(result.pending, 0);
    EXPECT_EQ(result.idle_milliseconds, -1);

    varn_runtime_free(runtime);
}

TEST(RuntimeBudget, GuardsItsArguments)
{
    varn_poll_result result{};
    EXPECT_EQ(varn_runtime_poll_budget(nullptr, kFrameBudgetNanoseconds, &result), 2);

    varn_runtime* runtime = varn_runtime_new();
    ASSERT_NE(runtime, nullptr);
    EXPECT_EQ(varn_runtime_poll_budget(runtime, -1, &result), 2);

    varn_runtime_stop(runtime);
    result = {1, 1, 0};
    EXPECT_EQ(varn_runtime_poll_budget(runtime, kFrameBudgetNanoseconds, &result), 0);
    EXPECT_EQ(result.ran, 0);
    EXPECT_EQ(result.idle_milliseconds, -1);
    varn_runtime_free(runtime);
}

// The same tasks under a budget leave every frame on time, and the tasks still wake from one frame to the next.
TEST(RuntimeBudget, ABudgetBoundsEveryPollOfSleepingTasks)
{
    varn_runtime* runtime = BudgetTestHelpers::start(BudgetTestHelpers::sleepers(kBoundedSleeperCount).c_str());
    ASSERT_NE(runtime, nullptr);

    long long longest = 0;
    varn_poll_result result{};
    for (int frame = 0; frame < 40; ++frame)
    {
        const auto started = Clock::now();
        EXPECT_EQ(varn_runtime_poll_budget(runtime, kFrameBudgetNanoseconds, &result), 1);
        longest = std::max<long long>(longest, std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count());
        EXPECT_EQ(result.pending, 1);
        std::this_thread::sleep_until(started + std::chrono::milliseconds(16));
    }

    std::printf("Measured: the longest budgeted poll over the sleeping tasks took %lld ms.\n", longest);
    EXPECT_LT(longest, kBoundMilliseconds);
    EXPECT_EQ(varn_runtime_load_string(runtime, "assert(wakes > 0, 'The sleeping tasks never woke.')", "=check"), 0);
    varn_runtime_free(runtime);
}

// A task that sleeps zero milliseconds arms a timer that waits for the next pass, so even a zero budget returns to the host.
TEST(RuntimeBudget, TimersArmedDuringAPassWaitForTheNextPass)
{
    varn_runtime* runtime = BudgetTestHelpers::start("local async = require('async') turns = 0 async.spawn(function() while true do async.sleep(0):await() turns = turns + 1 end end)");
    ASSERT_NE(runtime, nullptr);

    varn_poll_result result{};
    for (int poll = 0; poll < 10; ++poll)
    {
        EXPECT_EQ(varn_runtime_poll_budget(runtime, 0, &result), 1);
        EXPECT_EQ(result.ran, 1);
    }

    EXPECT_EQ(varn_runtime_load_string(runtime, "assert(turns >= 1 and turns <= 10, 'A zero budget ran ' .. turns .. ' turns in ten polls.')", "=check"), 0);

    const auto started = Clock::now();
    varn_runtime_poll_budget(runtime, kFrameBudgetNanoseconds, &result);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count(), kBoundMilliseconds);
    EXPECT_EQ(varn_runtime_load_string(runtime, "assert(turns > 10, 'A budget of a few milliseconds should run many turns.')", "=check"), 0);
    varn_runtime_free(runtime);
}

// A task that yields in a loop gives the host its poll back, and other tasks keep their turn between its yields.
TEST(RuntimeBudget, YieldGivesThePollBack)
{
    const char* const chunk =
        "local async = require('async')\n"
        "yields, ticks = 0, 0\n"
        "async.spawn(function() while true do async.yield():await() yields = yields + 1 end end)\n"
        "async.spawn(function() while true do async.sleep(1):await() ticks = ticks + 1 end end)\n";
    varn_runtime* runtime = BudgetTestHelpers::start(chunk);
    ASSERT_NE(runtime, nullptr);

    for (int poll = 0; poll < 10; ++poll)
    {
        EXPECT_EQ(varn_runtime_poll_budget(runtime, 0, nullptr), 1);
    }

    EXPECT_EQ(varn_runtime_load_string(runtime, "assert(yields >= 1 and yields <= 10, 'A zero budget ran ' .. yields .. ' yields in ten polls.')", "=check"), 0);

    BudgetTestHelpers::warm(runtime, true);

    EXPECT_EQ(varn_runtime_load_string(runtime, "assert(yields > 10 and ticks > 0, 'The yielding task starved the timer.')", "=check"), 0);
    varn_runtime_free(runtime);
}

// A sequential request reads its answer in the poll that sent it once the poll repeats its passes, instead of one step per frame.
TEST(RuntimeBudget, ASequentialRequestTakesFewerFrames)
{
    varn_runtime* single = BudgetTestHelpers::start(kSequentialRequests);
    ASSERT_NE(single, nullptr);
    BudgetTestHelpers::warm(single, false);
    const int singleFrames = BudgetTestHelpers::framesUntil(single, false, 20, 2000);
    varn_runtime_free(single);

    varn_runtime* budgeted = BudgetTestHelpers::start(kSequentialRequests);
    ASSERT_NE(budgeted, nullptr);
    BudgetTestHelpers::warm(budgeted, true);
    const int budgetedFrames = BudgetTestHelpers::framesUntil(budgeted, true, 20, 2000);
    varn_runtime_free(budgeted);

    std::printf("Measured: a sequential request took %.2f frames with one pass per poll and %.2f with a budget.\n", singleFrames / 20.0, budgetedFrames / 20.0);
    EXPECT_GE(singleFrames, 40);
    EXPECT_LE(budgetedFrames, 30);
}

// A stream moves as many bytes per frame as the budget allows instead of one read per frame.
TEST(RuntimeBudget, AStreamMovesFarMoreBytesPerFrame)
{
    const double single = BudgetTestHelpers::streamRate(kStream, false);
    const double budgeted = BudgetTestHelpers::streamRate(kStream, true);

    std::printf("Measured: a stream moved %.2f MiB/s with one pass per poll and %.2f MiB/s with a budget.\n", single, budgeted);
    EXPECT_GT(single, 0.0);
    EXPECT_GT(budgeted, single * kGain);
}

// An upload is read until the socket would block, up to a bound per readiness, and the budgeted poll keeps reading within the frame.
TEST(RuntimeBudget, AnUploadIsReadInFewFrames)
{
    varn_runtime* single = BudgetTestHelpers::start(kUploads);
    ASSERT_NE(single, nullptr);
    const int singleFrames = BudgetTestHelpers::framesUntil(single, false, 3, 2000);
    varn_runtime_free(single);

    varn_runtime* budgeted = BudgetTestHelpers::start(kUploads);
    ASSERT_NE(budgeted, nullptr);
    const int budgetedFrames = BudgetTestHelpers::framesUntil(budgeted, true, 3, 2000);
    varn_runtime_free(budgeted);

    std::printf("Measured: three uploads of 4 MiB took %d frames with one pass per poll and %d with a budget.\n", singleFrames, budgetedFrames);
    EXPECT_EQ(BudgetTestHelpers::reported, 3);
    EXPECT_GT(singleFrames, budgetedFrames);
}

// A listener serves every connection of a burst within a few frames, rather than one accept per frame.
TEST(RuntimeBudget, AListenerAcceptsABurstInFewFrames)
{
    varn_runtime* runtime = BudgetTestHelpers::start(kAcceptBurst);
    ASSERT_NE(runtime, nullptr);

    const int frames = BudgetTestHelpers::framesUntil(runtime, true, 100, 500);

    std::printf("Measured: a burst of 100 connections was accepted in %d frames with a budget.\n", frames);
    EXPECT_EQ(BudgetTestHelpers::reported, 100);
    EXPECT_LE(frames, 10);
    varn_runtime_free(runtime);
}

} // namespace varn::runtime
