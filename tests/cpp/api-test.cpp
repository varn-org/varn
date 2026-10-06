#include "varn/varn.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

namespace
{

std::string g_hostArgument;

extern "C" const char* echoHost(const char* json_argument, void* userdata)
{
    (void)userdata;
    g_hostArgument = json_argument != nullptr ? json_argument : "";
    return "{\"reply\":\"pong\"}";
}

TEST(CApi, VersionIsNonEmpty)
{
    const char* version = varn_version();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(std::strlen(version), 0u);
}

TEST(CApi, RunStringExecutesAndReturnsZero)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    EXPECT_EQ(varn_runtime_run_string(rt, "local x = 1 + 1", "test-chunk"), 0);

    varn_runtime_free(rt);
}

TEST(CApi, RunStringReportsAScriptError)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    EXPECT_NE(varn_runtime_run_string(rt, "this is not valid lua ==", "bad-chunk"), 0);

    varn_runtime_free(rt);
}

TEST(CApi, RegisteredHostFunctionRoundTripsThroughLua)
{
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    g_hostArgument.clear();
    EXPECT_EQ(varn_runtime_register(rt, "ping", &echoHost, nullptr), 0);

    const char* script =
        "local r = host.ping({ n = 42 })\n"
        "assert(r.reply == 'pong', 'The host result was not decoded.')\n";
    EXPECT_EQ(varn_runtime_run_string(rt, script, "host-test"), 0);
    EXPECT_NE(g_hostArgument.find("42"), std::string::npos);

    EXPECT_EQ(varn_runtime_register(rt, nullptr, &echoHost, nullptr), 2);
    EXPECT_EQ(varn_runtime_register(rt, "x", nullptr, nullptr), 2);
    EXPECT_EQ(varn_runtime_register(nullptr, "x", &echoHost, nullptr), 2);

    varn_runtime_free(rt);
}

TEST(CApi, StopFromAnotherThreadTearsDownAListeningServer)
{
    // A host can only stop from another thread, since `run_string` blocks while it drives the loop.
    // The script leaves behind the listening server and the I/O pool that the teardown then reaches across that boundary.
    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const char* script =
        "local http = require('http')\n"
        "local fs = require('fs')\n"
        "local async = require('async')\n"
        "http.createServer(function(req, res) res:send('ok') end)"
        ":listen({ host = '127.0.0.1', port = 0 })\n"
        "async.spawn(function()\n"
        "  while true do fs.readdir('.'):await() end\n"
        "end)\n";

    int exitCode = -1;
    // clang-format off
    std::thread runner([rt, script, &exitCode]
    {
        exitCode = varn_runtime_run_string(rt, script, "stop-test");
    });
    // clang-format on

    // Let the loop reach the listening state before the stop arrives.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    varn_runtime_stop(rt);
    runner.join();

    EXPECT_EQ(exitCode, 0);

    // A second stop is a no-op, and the free that follows must still join every thread cleanly.
    varn_runtime_stop(rt);
    varn_runtime_free(rt);
}

TEST(CApi, StopRacingTheChunkOfARunStringNeverWatchesAClosedSocket)
{
    // The stop lands before the chunk, while it arms its listener and once the loop serves it, so the listener it closes must never be watched afterwards.
    const char* script =
        "local http = require('http')\n"
        "http.createServer(function(req, res) res:send('ok') end)"
        ":listen({ host = '127.0.0.1', port = 0 })\n";

    for (int attempt = 0; attempt < 120; ++attempt)
    {
        varn_runtime* rt = varn_runtime_new();
        ASSERT_NE(rt, nullptr);

        int exitCode = -1;
        // clang-format off
        std::thread runner([rt, script, &exitCode]
        {
            exitCode = varn_runtime_run_string(rt, script, "stop-race");
        });
        // clang-format on

        std::this_thread::sleep_for(std::chrono::microseconds((attempt % 40) * 250));
        varn_runtime_stop(rt);
        runner.join();
        varn_runtime_free(rt);

        EXPECT_EQ(exitCode, 0);
    }
}

TEST(CApi, AStopWhileTheChunkStillRunsAfterArmingItsListenerEndsTheRun)
{
    // The chunk keeps running after its listener is armed, so the stop closes the listener before the loop ever runs.
    const char* script =
        "local http = require('http')\n"
        "http.createServer(function(req, res) res:send('ok') end)"
        ":listen({ host = '127.0.0.1', port = 0 })\n"
        "local started = os.clock()\n"
        "while os.clock() - started < 0.2 do end\n";

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    int exitCode = -1;
    // clang-format off
    std::thread runner([rt, script, &exitCode]
    {
        exitCode = varn_runtime_run_string(rt, script, "stop-in-chunk");
    });
    // clang-format on

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    varn_runtime_stop(rt);
    runner.join();
    varn_runtime_free(rt);

    EXPECT_EQ(exitCode, 0);
}

TEST(CApi, GuardsNullArguments)
{
    EXPECT_EQ(varn_runtime_run_file(nullptr, "x.lua"), 2);
    EXPECT_EQ(varn_runtime_run_string(nullptr, "x", "c"), 2);

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(varn_runtime_run_file(rt, nullptr), 2);
    EXPECT_EQ(varn_runtime_run_string(rt, nullptr, "c"), 2);
    varn_runtime_free(rt);

    // Freeing a null runtime is safe.
    varn_runtime_free(nullptr);
}

} // namespace
