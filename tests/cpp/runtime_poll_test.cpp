#include "varn/varn.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

namespace
{

std::string g_calls;

extern "C" const char* recordCall(const char* argument, void*)
{
    g_calls += argument != nullptr ? argument : "null";
    g_calls += ";";
    return "{\"id\":7}";
}

// Pumps the way a platform run loop would, giving up once nothing can make progress or the budget runs out.
int pumpUntilIdle(varn_runtime* runtime, int maxTicks)
{
    int ticks = 0;
    while (ticks < maxTicks && varn_runtime_poll(runtime) == 1)
    {
        ++ticks;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return ticks;
}

// The host owns its thread, so loading a chunk must run it and hand back control rather than taking the thread.
TEST(RuntimePoll, LoadRunsTheChunkWithoutEnteringTheLoop)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const auto started = std::chrono::steady_clock::now();
    const char* chunk = "local async = require('async') async.spawn(function() async.sleep(300):await() done = true end)";
    EXPECT_EQ(varn_runtime_load_string(rt, chunk, "=ui"), 0);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    // The sleep is still outstanding, so load must have returned well before it could have finished.
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 100);

    pumpUntilIdle(rt, 5000);
    EXPECT_EQ(varn_runtime_load_string(rt, "assert(done == true, 'the pump never ran the spawned work')", "=check"), 0);

    varn_runtime_free(rt);
}

// Every host call has to land on the thread that pumps, since that is the platform's ui thread in a real app.
TEST(RuntimePoll, HostCallsLandOnThePumpingThread)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);
    ASSERT_EQ(varn_runtime_register(rt, "ui", &recordCall, nullptr), 0);

    g_calls.clear();
    const char* chunk =
        "local async = require('async')\n"
        "host.ui({ widget = 'screen' })\n"
        "async.spawn(function()\n"
        "  async.sleep(20):await()\n"
        "  local answer = host.ui({ widget = 'button' })\n"
        "  assert(answer.id == 7, 'the host answer did not come back')\n"
        "end)\n";

    EXPECT_EQ(varn_runtime_load_string(rt, chunk, "=ui"), 0);
    EXPECT_NE(g_calls.find("screen"), std::string::npos);
    // The deferred call has not happened yet, because nothing has pumped.
    EXPECT_EQ(g_calls.find("button"), std::string::npos);

    pumpUntilIdle(rt, 5000);
    EXPECT_NE(g_calls.find("button"), std::string::npos);

    varn_runtime_free(rt);
}

// A host event is the direction a tap travels, and it must be delivered by the pump rather than by a blocking run.
TEST(RuntimePoll, DeliversHostEventsThroughThePump)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);
    ASSERT_EQ(varn_runtime_register(rt, "ui", &recordCall, nullptr), 0);

    g_calls.clear();
    EXPECT_EQ(varn_runtime_load_string(rt, "host.on('tap', function(p) host.ui(p) end)", "=ui"), 0);

    EXPECT_EQ(varn_runtime_emit(rt, "tap", "{\"id\":\"save\"}"), 0);
    pumpUntilIdle(rt, 100);
    EXPECT_NE(g_calls.find("save"), std::string::npos);

    // A second tap arriving later is delivered by a later tick, which is what an idle app looks like.
    EXPECT_EQ(varn_runtime_emit(rt, "tap", "{\"id\":\"open\"}"), 0);
    pumpUntilIdle(rt, 100);
    EXPECT_NE(g_calls.find("open"), std::string::npos);

    varn_runtime_free(rt);
}

// Polling a runtime the host has stopped must answer that nothing more can happen instead of touching Lua.
TEST(RuntimePoll, StopEndsThePump)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    EXPECT_EQ(varn_runtime_load_string(rt, "host.on('tap', function() end)", "=ui"), 0);
    varn_runtime_stop(rt);

    EXPECT_EQ(varn_runtime_poll(rt), 0);
    EXPECT_EQ(varn_runtime_emit(rt, "tap", "null"), 0);
    EXPECT_EQ(varn_runtime_poll(rt), 0);

    varn_runtime_free(rt);
}

// Sockets are serviced by libuv rather than by the job queue, so the pump has to advance them too.
TEST(RuntimePoll, DrivesSocketWorkAsWellAsTimers)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);
    ASSERT_EQ(varn_runtime_register(rt, "ui", &recordCall, nullptr), 0);

    g_calls.clear();
    const char* const serving =
        "local http = require('http')\n"
        "local async = require('async')\n"
        "local app = http.createApp()\n"
        "app:get('/ping', function(ctx) ctx:text('pong') end)\n"
        "app:listen({ host = '127.0.0.1', port = 39881 })\n"
        "async.spawn(function()\n"
        "  local res = http.client.get('http://127.0.0.1:39881/ping'):await()\n"
        "  host.ui({ answered = res.body })\n"
        "end)\n";

    EXPECT_EQ(varn_runtime_load_string(rt, serving, "=serving"), 0);

    // A listening server keeps the pump busy forever, so this runs until the answer lands rather than until idle.
    for (int tick = 0; tick < 5000 && g_calls.find("pong") == std::string::npos; ++tick)
    {
        varn_runtime_poll(rt);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_NE(g_calls.find("pong"), std::string::npos);

    varn_runtime_free(rt);
}

TEST(RuntimePoll, GuardsNullArguments)
{
    EXPECT_EQ(varn_runtime_poll(nullptr), 2);
    EXPECT_EQ(varn_runtime_load_file(nullptr, "x.lua"), 2);
    EXPECT_EQ(varn_runtime_load_string(nullptr, "local a = 1", "=x"), 2);
}

} // namespace
