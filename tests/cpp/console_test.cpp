#include "varn/log/Log.h"
#include "varn/varn.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace
{

std::vector<std::pair<int, std::string>> g_lines;

extern "C" void collect(int level, const char* message, void*)
{
    g_lines.emplace_back(level, message != nullptr ? message : "");
}

// A host installs one sink and sees everything the script writes, which is what removes the need for a harness.
TEST(Console, DeliversScriptOutputWithItsLevel)
{
    g_lines.clear();
    varn_set_console(&collect, nullptr);

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const char* chunk =
        "print('plain', 1, true)\n"
        "local log = require('log')\n"
        "log.warn('Careful')\n"
        "log.error('Failed')\n";

    EXPECT_EQ(varn_runtime_run_string(rt, chunk, "=console"), 0);
    varn_runtime_free(rt);
    varn_set_console(nullptr, nullptr);

    ASSERT_GE(g_lines.size(), 3u);

    // A printed line arrives at the info level, since it is not a diagnostic with a severity of its own.
    EXPECT_EQ(g_lines[0].first, 1);
    EXPECT_EQ(g_lines[0].second, "plain\t1\ttrue");

    bool sawWarn = false;
    bool sawError = false;
    for (const auto& [level, message] : g_lines)
    {
        sawWarn = sawWarn || (level == 2 && message.find("Careful") != std::string::npos);
        sawError = sawError || (level == 3 && message.find("Failed") != std::string::npos);
    }

    EXPECT_TRUE(sawWarn);
    EXPECT_TRUE(sawError);
}

// A log line reaches the host through the logger, so it carries the level and name the pattern renders.
TEST(Console, LogLinesArriveFormattedWhileScriptOutputStaysRaw)
{
    g_lines.clear();
    varn_set_console(&collect, nullptr);

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);
    EXPECT_EQ(varn_runtime_run_string(rt, "print('raw') require('log').warn('Shaped')", "=console"), 0);
    varn_runtime_free(rt);
    varn_set_console(nullptr, nullptr);

    ASSERT_GE(g_lines.size(), 2u);

    // The script output is exactly what the script wrote, with nothing added around it.
    EXPECT_EQ(g_lines[0].second, "raw");

    const std::string logged = g_lines[1].second;
    EXPECT_NE(logged.find("Shaped"), std::string::npos);
    EXPECT_NE(logged.find("varn"), std::string::npos) << "The log line lost the logger pattern: " << logged;
    EXPECT_NE(logged.find("warning"), std::string::npos) << "The log line lost its level: " << logged;
}

// A line reaches the host as the script writes it rather than after the chunk has finished.
TEST(Console, ArrivesWhileTheChunkIsStillRunning)
{
    g_lines.clear();
    varn_set_console(&collect, nullptr);

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const char* chunk =
        "local async = require('async')\n"
        "print('before')\n"
        "async.spawn(function() async.sleep(50):await() print('after') end)\n";

    EXPECT_EQ(varn_runtime_load_string(rt, chunk, "=console"), 0);
    // The spawned half has not run yet, because nothing has pumped the loop.
    EXPECT_EQ(g_lines.size(), 1u);
    EXPECT_EQ(g_lines[0].second, "before");

    while (varn_runtime_poll(rt) == 1)
    {
    }

    ASSERT_EQ(g_lines.size(), 2u);
    EXPECT_EQ(g_lines[1].second, "after");

    varn_runtime_free(rt);
    varn_set_console(nullptr, nullptr);
}

// Raising the level silences diagnostics, and a script that prints its result must never go quiet with them.
TEST(Console, RaisingTheLogLevelNeverSilencesPrint)
{
    g_lines.clear();
    varn_set_console(&collect, nullptr);

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const char* chunk =
        "local log = require('log')\n"
        "log.setLevel('error')\n"
        "log.info('This diagnostic is below the level')\n"
        "print('This result still matters')\n"
        "log.error('This diagnostic is at the level')\n";

    EXPECT_EQ(varn_runtime_run_string(rt, chunk, "=console"), 0);
    varn_runtime_free(rt);
    varn_set_console(nullptr, nullptr);

    bool sawInfo = false;
    bool sawPrint = false;
    bool sawError = false;
    for (const auto& [level, message] : g_lines)
    {
        sawInfo = sawInfo || message.find("below the level") != std::string::npos;
        sawPrint = sawPrint || message == "This result still matters";
        sawError = sawError || message.find("at the level") != std::string::npos;
    }

    EXPECT_FALSE(sawInfo) << "A diagnostic below the level was still delivered.";
    EXPECT_TRUE(sawPrint) << "Raising the log level silenced what the script printed.";
    EXPECT_TRUE(sawError);
}

TEST(Console, DetachingTheSinkStopsDelivery)
{
    varn_set_console(&collect, nullptr);
    varn_set_console(nullptr, nullptr);

    g_lines.clear();
    varn_runtime* rt = varn_runtime_new();
    EXPECT_EQ(varn_runtime_run_string(rt, "print('Nobody listens')", "=console"), 0);
    varn_runtime_free(rt);

    EXPECT_TRUE(g_lines.empty());
}

// The loggers outlive the exit of the process, so a thread still writing while another thread runs the exit never locks a destroyed mutex.
TEST(ConsoleDeathTest, AThreadStillWritingWhileAnotherExitsEndsCleanly)
{
    // clang-format off
    EXPECT_EXIT(
    {
        varn::log::Log::setSink([](varn::log::Level, std::string_view) {});
        std::atomic<bool> writing{false};
        std::thread writer([&writing]
        {
            for (;;)
            {
                varn::log::Log::emit(varn::log::Level::Warn, "Still writing");
                writing.store(true);
            }
        });
        writer.detach();

        while (!writing.load())
        {
        }

        std::thread exiting([] { std::exit(0); });
        exiting.join();
    }, ::testing::ExitedWithCode(0), "");
    // clang-format on
}

} // namespace
