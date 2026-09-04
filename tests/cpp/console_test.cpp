#include "varn/varn.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

std::vector<std::pair<int, std::string>> g_lines;

extern "C" void collect(int level, const char* message, void*)
{
    g_lines.emplace_back(level, message != nullptr ? message : "");
}

// A host installs one sink and sees everything the script writes, which is what removes the need for a harness
TEST(Console, DeliversScriptOutputWithItsLevel)
{
    g_lines.clear();
    varn_set_console(&collect, nullptr);

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const char* chunk =
        "print('plain', 1, true)\n"
        "local log = require('log')\n"
        "log.warn('careful')\n"
        "log.error('failed')\n";

    EXPECT_EQ(varn_runtime_run_string(rt, chunk, "=console"), 0);
    varn_runtime_free(rt);
    varn_set_console(nullptr, nullptr);

    ASSERT_GE(g_lines.size(), 3u);

    // print carries the log level, which is what a browser console calls console.log
    EXPECT_EQ(g_lines[0].first, 0);
    EXPECT_EQ(g_lines[0].second, "plain\t1\ttrue");

    bool sawWarn = false;
    bool sawError = false;
    for (const auto& [level, message] : g_lines)
    {
        sawWarn = sawWarn || (level == 3 && message.find("careful") != std::string::npos);
        sawError = sawError || (level == 4 && message.find("failed") != std::string::npos);
    }

    EXPECT_TRUE(sawWarn);
    EXPECT_TRUE(sawError);
}

// A line reaches the host as the script writes it rather than after the chunk has finished
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
    // the spawned half has not run yet, because nothing has pumped the loop
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

TEST(Console, DetachingTheSinkStopsDelivery)
{
    varn_set_console(&collect, nullptr);
    varn_set_console(nullptr, nullptr);

    g_lines.clear();
    varn_runtime* rt = varn_runtime_new();
    EXPECT_EQ(varn_runtime_run_string(rt, "print('nobody listens')", "=console"), 0);
    varn_runtime_free(rt);

    EXPECT_TRUE(g_lines.empty());
}

} // namespace
