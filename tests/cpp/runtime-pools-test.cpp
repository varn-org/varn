#include "varn/async/Promise.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/Scheduler.h"
#include "varn/varn.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace varn::runtime
{

namespace
{

using Clock = std::chrono::steady_clock;

constexpr const char* kReadsChunk = "local async = require('async')\n"
                                    "local fs = require('fs')\n"
                                    "async.run(function()\n"
                                    "  local reads = {}\n"
                                    "  for i = 1, 20 do reads[i] = fs.stat('.') end\n"
                                    "  for i = 1, 20 do assert(reads[i]:await().isDir) end\n"
                                    "end)\n";

} // namespace

// A loop with no socket never asks the system whether one is ready, so a task that yields in a loop turns without an empty poll, which macOS makes far slower than the turn itself.
TEST(EventLoop, ATurnWithoutSocketsSkipsTheSystemPoll)
{
#if !defined(NDEBUG) || defined(VARN_TESTS_SANITIZED)
    GTEST_SKIP() << "The cost of a turn is measured only in an optimized build without sanitizers.";
#endif
#if defined(_MSC_VER)
    GTEST_SKIP() << "A yield unwinds as a C++ exception under MSVC, so the cost of a turn is measured only where it jumps.";
#endif

    varn_runtime* rt = varn_runtime_new();
    ASSERT_NE(rt, nullptr);

    const auto started = Clock::now();
    EXPECT_EQ(varn_runtime_run_string(rt, "local async = require('async') async.run(function() for i = 1, 50000 do async.yield():await() end end)", "=turns"), 0);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();

    std::printf("Measured: 50000 turns of a task that yields took %lld ms.\n", static_cast<long long>(elapsed));
    EXPECT_LT(elapsed, 250);
    varn_runtime_free(rt);
}

// The C API creates a runtime with the pools its options name, and refuses options it was not given.
TEST(Runtime, TheCApiCreatesARuntimeWithItsOptions)
{
    EXPECT_EQ(varn_runtime_new_with_options(nullptr), nullptr);

    const varn_runtime_options options{1, 1};
    varn_runtime* rt = varn_runtime_new_with_options(&options);
    ASSERT_NE(rt, nullptr);

    EXPECT_EQ(varn_runtime_run_string(rt, kReadsChunk, "=reads"), 0);
    varn_runtime_free(rt);
}

// A promise no coroutine waits on posts no resume once it settles, while a rejection no one observed still reaches the failure handler through the loop.
TEST(Runtime, ASettledPromiseWithoutAWaiterPostsNothing)
{
    Scheduler scheduler;
    Runtime runtime(scheduler, std::vector<std::string>{"varn"});
    ASSERT_FALSE(scheduler.mainLoop().hasPendingJobs());

    auto resolved = std::make_shared<async::Promise>(runtime);
    resolved->resolve("value");
    EXPECT_FALSE(scheduler.mainLoop().hasPendingJobs());

    auto reported = std::make_shared<async::Promise>(runtime);
    reported->rejectReported("The failure was already reported.");
    EXPECT_FALSE(scheduler.mainLoop().hasPendingJobs());

    auto rejected = std::make_shared<async::Promise>(runtime);
    rejected->reject("Nobody observed this rejection.");
    EXPECT_TRUE(scheduler.mainLoop().hasPendingJobs());
}

} // namespace varn::runtime
