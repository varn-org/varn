#include "varn/varn.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace
{

const char* const kSleepingChunk =
    "local async = require('async')\n"
    "async.run(function()\n"
    "  async.sleep(200):await()\n"
    "  ran = (ran or 0) + 1\n"
    "end)\n";

long long runAndMeasure(varn_runtime* runtime, const char* source, const char* chunkName, int& code)
{
    const auto started = std::chrono::steady_clock::now();
    code = varn_runtime_run_string(runtime, source, chunkName);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

// a host that loads a chunk and then runs another must see the loop drive both, not silently skip the second
TEST(RuntimeLifetime, EveryChunkGetsTheEventLoop)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    int first = -1;
    int second = -1;
    const long long firstMs = runAndMeasure(rt, kSleepingChunk, "=first", first);
    const long long secondMs = runAndMeasure(rt, kSleepingChunk, "=second", second);

    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 0);
    EXPECT_GE(firstMs, 150);
    EXPECT_GE(secondMs, 150);

    EXPECT_EQ(varn_runtime_run_string(rt, "assert(ran == 2, 'both chunks must have run')", "=check"), 0);

    varn_runtime_free(rt);
}

// the exit code belongs to the chunk that produced it, so a failure must not decide the outcome of the next one
TEST(RuntimeLifetime, AFailedChunkDoesNotPoisonTheNext)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const char* const failingEntry = "local async = require('async') async.run(function() error('boom') end)";
    EXPECT_NE(varn_runtime_run_string(rt, failingEntry, "=failing"), 0);
    EXPECT_EQ(varn_runtime_run_string(rt, "local ok = 1", "=healthy"), 0);

    varn_runtime_free(rt);
}

// an entry that asks the loop to stop ends its own chunk, and the next chunk still gets a running loop
TEST(RuntimeLifetime, AnEntryThatStopsTheLoopDoesNotEndTheRuntime)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    EXPECT_EQ(varn_runtime_run_string(rt, "local async = require('async') async.run(function() end)", "=stopper"), 0);

    int code = -1;
    const long long elapsed = runAndMeasure(rt, kSleepingChunk, "=after", code);
    EXPECT_EQ(code, 0);
    EXPECT_GE(elapsed, 150);

    varn_runtime_free(rt);
}

// socket state is owned by the loop, so a second chunk that binds a server proves the io layer survived the first
TEST(RuntimeLifetime, SocketsStillWorkAfterAnEarlierChunkFinished)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    EXPECT_EQ(varn_runtime_run_string(rt, kSleepingChunk, "=warmup"), 0);

    const char* const serving =
        "local http = require('http')\n"
        "local async = require('async')\n"
        "local app = http.createApp()\n"
        "app:get('/ping', function(ctx) ctx:text('pong') end)\n"
        "app:listen({ host = '127.0.0.1', port = 39877 })\n"
        "async.run(function()\n"
        "  local res = http.client.get('http://127.0.0.1:39877/ping'):await()\n"
        "  assert(res.body == 'pong', 'the server answered ' .. tostring(res.body))\n"
        "end)\n";

    EXPECT_EQ(varn_runtime_run_string(rt, serving, "=serving"), 0);

    varn_runtime_free(rt);
}

// a host retains to keep the loop waiting for its events, so a retain must never be lost to the exit decision
TEST(RuntimeLifetime, ARetainHeldAcrossAChunkKeepsTheLoopRunning)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    ASSERT_EQ(varn_runtime_retain(rt), 0);

    std::atomic<bool> entered{false};
    std::thread releaser(
        [rt, &entered]
        {
            while (!entered.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            varn_runtime_release(rt);
        });

    entered.store(true);
    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(varn_runtime_run_string(rt, "local held = true", "=retained"), 0);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    releaser.join();
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 200);

    varn_runtime_free(rt);
}

// releasing what was never retained would drive the count below zero and leave the loop unable to ever exit
TEST(RuntimeLifetime, AnUnbalancedReleaseIsRefused)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    EXPECT_NE(varn_runtime_release(rt), 0);

    EXPECT_EQ(varn_runtime_retain(rt), 0);
    EXPECT_EQ(varn_runtime_release(rt), 0);
    EXPECT_NE(varn_runtime_release(rt), 0);

    EXPECT_EQ(varn_runtime_run_string(rt, "local ok = 1", "=exits"), 0);

    varn_runtime_free(rt);
}

} // namespace
