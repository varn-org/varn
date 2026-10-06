#include "varn/varn.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
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

extern "C" void countWake(void* userdata)
{
    static_cast<std::atomic<int>*>(userdata)->fetch_add(1);
}

// Keeps the port a server announces as `{"port":N}`, so a runtime of another thread can reach it.
extern "C" const char* recordPort(const char* argument, void* userdata)
{
    const char* colon = argument != nullptr ? std::strchr(argument, ':') : nullptr;
    static_cast<std::atomic<int>*>(userdata)->store(colon != nullptr ? std::atoi(colon + 1) : -1);
    return "null";
}

// Waits the way a host sleeping in its own run loop would, until a wake arrives or the budget runs out.
bool waitForWake(const std::atomic<int>& wakes, int before)
{
    for (int tick = 0; tick < 5000 && wakes.load() == before; ++tick)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return wakes.load() > before;
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

    // The sleep is still outstanding, so `load` must have returned well before it could have finished.
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 100);

    pumpUntilIdle(rt, 5000);
    EXPECT_EQ(varn_runtime_load_string(rt, "assert(done == true, 'The pump never ran the spawned work.')", "=check"), 0);

    varn_runtime_free(rt);
}

// Every host call has to land on the thread that pumps, since that is the platform's UI thread in a real app.
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
        "  assert(answer.id == 7, 'The host answer did not come back.')\n"
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
    EXPECT_EQ(varn_runtime_idle(rt), -1);
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
        "local port = app:listen({ host = '127.0.0.1', port = 0 }).port\n"
        "async.spawn(function()\n"
        "  local res = http.client.get('http://127.0.0.1:' .. port .. '/ping'):await()\n"
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

// A host that sleeps until the next timer instead of ticking needs to know how long that is, and that nothing is armed once it fired.
TEST(RuntimePoll, IdleAnswersTheTimeToTheNextTimer)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(varn_runtime_idle(rt), -1);

    EXPECT_EQ(varn_runtime_load_string(rt, "local async = require('async') async.spawn(function() async.sleep(300):await() done = true end)", "=ui"), 0);
    varn_runtime_poll(rt);
    const long long idle = varn_runtime_idle(rt);
    EXPECT_GT(idle, 200);
    EXPECT_LE(idle, 300);

    std::this_thread::sleep_for(std::chrono::milliseconds(idle));
    EXPECT_EQ(varn_runtime_idle(rt), 0);
    varn_runtime_poll(rt);
    EXPECT_EQ(varn_runtime_load_string(rt, "assert(done == true, 'The timer did not fire at its deadline.')", "=check"), 0);
    EXPECT_EQ(varn_runtime_idle(rt), -1);

    varn_runtime_free(rt);
}

// A deadline raced with work that settled first leaves no timer, so a host that polls is not woken for a deadline nobody waits for.
TEST(RuntimePoll, LeavesNoTimerBehindADeadlineThatWasMet)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    EXPECT_EQ(varn_runtime_load_string(rt, "local async = require('async') async.spawn(function() async.timeout(async.sleep(5), 60000):await() done = true end)", "=ui"), 0);
    pumpUntilIdle(rt, 5000);
    EXPECT_EQ(varn_runtime_load_string(rt, "assert(done == true, 'The work under the deadline never settled.')", "=check"), 0);
    EXPECT_EQ(varn_runtime_idle(rt), -1);

    varn_runtime_free(rt);
}

// A combinator waits for its inputs without polling, so a host sleeps until the input that settles first.
TEST(RuntimePoll, CombinatorsSleepUntilTheirInputsSettle)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const char* chunk = "local async = require('async') async.spawn(function() async.all({ async.race({ async.sleep(300) }), async.any({ async.sleep(300) }), async.allSettled({ async.sleep(300) }) }):await() done = true end)";
    EXPECT_EQ(varn_runtime_load_string(rt, chunk, "=ui"), 0);
    varn_runtime_poll(rt);
    EXPECT_GT(varn_runtime_idle(rt), 200);

    pumpUntilIdle(rt, 5000);
    EXPECT_EQ(varn_runtime_load_string(rt, "assert(done == true, 'The combinators never settled.')", "=check"), 0);
    EXPECT_EQ(varn_runtime_idle(rt), -1);

    varn_runtime_free(rt);
}

// A promise a pool thread settles and an event emitted from another thread both reach a host that sleeps instead of polling.
TEST(RuntimePoll, WakesTheHostWhenWorkArrivesFromAnotherThread)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);
    ASSERT_EQ(varn_runtime_register(rt, "ui", &recordCall, nullptr), 0);

    // The first poll makes this the thread of the loop, so only work from other threads wakes the host.
    varn_runtime_poll(rt);
    std::atomic<int> wakes{0};
    varn_runtime_set_wake(rt, &countWake, &wakes);
    g_calls.clear();
    const char* chunk =
        "local async = require('async')\n"
        "local fs = require('fs')\n"
        "host.on('tap', function(p) host.ui(p) end)\n"
        "async.spawn(function()\n"
        "  local info = fs.stat('.'):await()\n"
        "  host.ui({ statted = info ~= nil })\n"
        "end)\n";

    // A read the worker settles after the task awaits it wakes the host, while one it settles first was already read in place by the task, which leaves the loop nothing to do.
    EXPECT_EQ(varn_runtime_load_string(rt, chunk, "=ui"), 0);
    if (g_calls.find("statted") == std::string::npos)
    {
        ASSERT_TRUE(waitForWake(wakes, 0));
    }

    pumpUntilIdle(rt, 100);
    EXPECT_NE(g_calls.find("statted"), std::string::npos);
    EXPECT_EQ(varn_runtime_idle(rt), -1);

    const int beforeTap = wakes.load();
    std::thread tapping([rt]
                        { varn_runtime_emit(rt, "tap", "{\"id\":\"save\"}"); });
    tapping.join();
    ASSERT_TRUE(waitForWake(wakes, beforeTap));
    EXPECT_EQ(varn_runtime_idle(rt), 0);
    pumpUntilIdle(rt, 100);
    EXPECT_NE(g_calls.find("save"), std::string::npos);

    // Once the host takes its function back, work from another thread is left for the next poll without calling it.
    varn_runtime_set_wake(rt, nullptr, nullptr);
    const int afterRelease = wakes.load();
    std::thread late([rt]
                     { varn_runtime_emit(rt, "tap", "{\"id\":\"late\"}"); });
    late.join();
    EXPECT_EQ(wakes.load(), afterRelease);
    pumpUntilIdle(rt, 100);
    EXPECT_NE(g_calls.find("late"), std::string::npos);

    varn_runtime_free(rt);
}

// A stream that finished before Lua awaited it still runs its head and every chunk before the await answers.
TEST(RuntimePoll, SettlesAStreamAfterItsHeadAndChunks)
{
    varn_runtime* server = varn_runtime_new();
    ASSERT_NE(server, nullptr);
    std::atomic<int> port{0};
    ASSERT_EQ(varn_runtime_register(server, "ready", &recordPort, &port), 0);
    const char* const serving =
        "local http = require('http')\n"
        "local app = http.createApp()\n"
        "app:get('/events', function(ctx) local s = ctx:sse() s:send('one') s:send('two') s:close() end)\n"
        "local server = app:listen({ host = '127.0.0.1', port = 0 })\n"
        "host.ready({ port = server.port })\n";
    std::thread serverThread([server, serving]
                             { varn_runtime_run_string(server, serving, "=server"); });

    for (int tick = 0; tick < 5000 && port.load() == 0; ++tick)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_GT(port.load(), 0);

    varn_runtime* client = varn_runtime_new();
    ASSERT_NE(client, nullptr);
    ASSERT_EQ(varn_runtime_register(client, "ui", &recordCall, nullptr), 0);
    g_calls.clear();
    const std::string streaming = "local http = require('http')\n"
                                  "seen = {}\n"
                                  "local options = { url = 'http://127.0.0.1:" +
                                  std::to_string(port.load()) +
                                  "/events', method = 'GET', headers = {}, timeoutSeconds = 10 }\n"
                                  "stream = http.client.streamRaw(options, function() seen[#seen + 1] = 'chunk' end, function() seen[#seen + 1] = 'head' end)\n";
    EXPECT_EQ(varn_runtime_load_string(client, streaming.c_str(), "=stream"), 0);

    // Nothing pumps the client while its request runs, so the head, the chunks and the end all wait on its loop.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // The await happens right away rather than from a later job, which is where an early end would overtake the chunks.
    const char* const awaiting =
        "coroutine.wrap(function()\n"
        "  local done = stream:await()\n"
        "  host.ui({ done = done, seen = table.concat(seen, ',') })\n"
        "end)()\n";
    EXPECT_EQ(varn_runtime_load_string(client, awaiting, "=await"), 0);
    pumpUntilIdle(client, 5000);

    EXPECT_NE(g_calls.find("\"done\":\"ok\""), std::string::npos) << g_calls;
    EXPECT_NE(g_calls.find("head,chunk"), std::string::npos) << g_calls;

    varn_runtime_free(client);
    varn_runtime_stop(server);
    serverThread.join();
    varn_runtime_free(server);
}

TEST(RuntimePoll, GuardsNullArguments)
{
    EXPECT_EQ(varn_runtime_poll(nullptr), 2);
    EXPECT_EQ(varn_runtime_load_file(nullptr, "x.lua"), 2);
    EXPECT_EQ(varn_runtime_load_string(nullptr, "local a = 1", "=x"), 2);
    EXPECT_EQ(varn_runtime_idle(nullptr), -1);
    varn_runtime_set_wake(nullptr, &countWake, nullptr);
}

} // namespace
