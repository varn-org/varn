#include "varn/async/Promise.h"
#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"
#include "varn/runtime/WorkLedger.h"
#include "varn/varn.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace varn::runtime
{

namespace
{

using Clock = std::chrono::steady_clock;

constexpr std::chrono::milliseconds kKeepAlive{10000};

class PoolsTestHelpers
{
public:
    static bool waitForThreads(const TaskPool& pool, std::size_t expected)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (pool.threadCount() != expected)
        {
            if (Clock::now() >= deadline)
            {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }

    static bool waitForIdleThreads(const TaskPool& pool, std::size_t expected)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (pool.idleThreadCount() != expected)
        {
            if (Clock::now() >= deadline)
            {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }

    static bool waitFor(const std::atomic<int>& value, int expected)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (value.load(std::memory_order_acquire) < expected)
        {
            if (Clock::now() >= deadline)
            {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }

    // Answers the most jobs the pool ran at once, holding every job until as many as the pool should run are running and a moment longer.
    static int peakConcurrency(TaskPool& pool, int threads, int jobs)
    {
        std::atomic<int> active{0};
        std::atomic<int> peak{0};
        std::atomic<int> finished{0};
        std::atomic<bool> release{false};

        for (int index = 0; index < jobs; ++index)
        {
            // clang-format off
            pool.post([&active, &peak, &finished, &release]
            {
                const int now = active.fetch_add(1, std::memory_order_acq_rel) + 1;
                int seen = peak.load(std::memory_order_acquire);
                while (now > seen && !peak.compare_exchange_weak(seen, now, std::memory_order_acq_rel))
                {
                }

                while (!release.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                active.fetch_sub(1, std::memory_order_acq_rel);
                finished.fetch_add(1, std::memory_order_acq_rel);
            });
            // clang-format on
        }

        EXPECT_TRUE(waitFor(peak, threads));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const int reached = peak.load(std::memory_order_acquire);

        release.store(true, std::memory_order_release);
        EXPECT_TRUE(waitFor(finished, jobs));
        return reached;
    }
};

constexpr const char* kReadsChunk = "local async = require('async')\n"
                                    "local fs = require('fs')\n"
                                    "async.run(function()\n"
                                    "  local reads = {}\n"
                                    "  for i = 1, 20 do reads[i] = fs.stat('.') end\n"
                                    "  for i = 1, 20 do assert(reads[i]:await().isDir) end\n"
                                    "end)\n";

} // namespace

// A worker that is busy takes the next job from the queue by itself, so jobs posted while no worker waits still all run once one frees.
TEST(TaskPool, JobsPostedWhileEveryWorkerIsBusyRunOnceOneFrees)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(2, kKeepAlive, ledger);

    std::atomic<int> blocked{0};
    std::atomic<bool> release{false};
    for (int index = 0; index < 2; ++index)
    {
        // clang-format off
        pool.post([&blocked, &release]
        {
            blocked.fetch_add(1, std::memory_order_acq_rel);
            while (!release.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        // clang-format on
    }

    ASSERT_TRUE(PoolsTestHelpers::waitFor(blocked, 2));

    std::atomic<int> ran{0};
    constexpr int kJobs = 500;
    for (int index = 0; index < kJobs; ++index)
    {
        // clang-format off
        pool.post([&ran]
        {
            ran.fetch_add(1, std::memory_order_relaxed);
        });
        // clang-format on
    }

    release.store(true, std::memory_order_release);
    ASSERT_TRUE(PoolsTestHelpers::waitFor(ran, kJobs));
    pool.stop();

    EXPECT_EQ(ran.load(), kJobs);
    EXPECT_EQ(ledger->depth(), 0);
}

// Producers on many threads and jobs that post more jobs from the workers all race the idle count, and no job is left in the queue unseen.
TEST(TaskPool, PostsFromProducersAndWorkersLoseNoJob)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(3, kKeepAlive, ledger);

    std::atomic<int> ran{0};
    constexpr int kProducers = 6;
    constexpr int kPerProducer = 3000;

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int producer = 0; producer < kProducers; ++producer)
    {
        // clang-format off
        producers.emplace_back([&pool, &ran]
        {
            for (int index = 0; index < kPerProducer; ++index)
            {
                pool.post([&pool, &ran]
                {
                    ran.fetch_add(1, std::memory_order_relaxed);
                    pool.post([&ran] { ran.fetch_add(1, std::memory_order_relaxed); });
                });

                if (index % 64 == 0)
                {
                    std::this_thread::yield();
                }
            }
        });
        // clang-format on
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    ASSERT_TRUE(PoolsTestHelpers::waitFor(ran, kProducers * kPerProducer * 2));
    while (ledger->depth() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    pool.stop();
    EXPECT_EQ(ran.load(), kProducers * kPerProducer * 2);
}

// The last entry that leaves the ledger from a pool thread wakes a loop that waits for it, so the loop exits as soon as the work ends rather than at its next self-check.
TEST(EventLoop, ExitsOnceTheLastPoolJobLeaves)
{
    auto ledger = std::make_shared<WorkLedger>();
    EventLoop loop(ledger);
    // clang-format off
    ledger->setNotify([&loop]
    {
        loop.wake();
    });
    loop.setIdleExitPredicate([&ledger]
    {
        return ledger->depth() == 0;
    });
    // clang-format on

    TaskPool pool(1, kKeepAlive, ledger);
    // clang-format off
    pool.post([]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    });
    // clang-format on

    const auto started = Clock::now();
    loop.enter();
    loop.run();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();

    pool.stop();
    EXPECT_EQ(ledger->depth(), 0);
    EXPECT_LT(elapsed, 600);
}

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

// A pool starts no thread before its first job, so a runtime that never touches a pool costs no thread for it.
TEST(TaskPool, StartsNoThreadBeforeItsFirstJob)
{
    Runtime runtime(std::vector<std::string>{"varn"});

    EXPECT_EQ(runtime.taskPool().threadCount(), 0u);
    EXPECT_EQ(runtime.ioPool().threadCount(), 0u);
}

// A job posted once the worker went idle again reuses it, so a pool grows only when every worker it has is busy.
TEST(TaskPool, ReusesAnIdleWorkerBeforeItStartsAnother)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(8, kKeepAlive, ledger);

    for (int index = 0; index < 50; ++index)
    {
        std::atomic<int> ran{0};
        // clang-format off
        pool.post([&ran]
        {
            ran.fetch_add(1, std::memory_order_acq_rel);
        });
        // clang-format on

        ASSERT_TRUE(PoolsTestHelpers::waitFor(ran, 1));
        ASSERT_TRUE(PoolsTestHelpers::waitForIdleThreads(pool, 1));
    }

    EXPECT_EQ(pool.threadCount(), 1u);
    pool.stop();
}

// Jobs that block take a thread each up to the limit and never one more, and the jobs past the limit wait for a worker to free.
TEST(TaskPool, GrowsToItsLimitAndNoFurther)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(4, kKeepAlive, ledger);

    EXPECT_EQ(PoolsTestHelpers::peakConcurrency(pool, 4, 8), 4);
    EXPECT_LE(pool.threadCount(), 4u);
    pool.stop();
    EXPECT_EQ(ledger->depth(), 0);
}

// A worker that stays idle for the keep-alive ends, and a job that comes later starts a worker again.
TEST(TaskPool, AnIdleWorkerEndsAfterItsKeepAlive)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(4, std::chrono::milliseconds(20), ledger);

    EXPECT_EQ(PoolsTestHelpers::peakConcurrency(pool, 4, 8), 4);
    ASSERT_TRUE(PoolsTestHelpers::waitForThreads(pool, 0));

    std::atomic<int> ran{0};
    // clang-format off
    pool.post([&ran]
    {
        ran.fetch_add(1, std::memory_order_acq_rel);
    });
    // clang-format on

    ASSERT_TRUE(PoolsTestHelpers::waitFor(ran, 1));
    pool.stop();
    EXPECT_EQ(pool.threadCount(), 0u);
}

// Workers that end on their keep-alive while jobs keep arriving lose no job and leave no thread behind, which is the race of a retiring worker against a post that would reuse it.
TEST(TaskPool, WorkersThatRetireWhileJobsArriveLoseNoJob)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(6, std::chrono::milliseconds(1), ledger);

    std::atomic<int> ran{0};
    constexpr int kRounds = 200;
    constexpr int kPerRound = 8;
    for (int round = 0; round < kRounds; ++round)
    {
        for (int index = 0; index < kPerRound; ++index)
        {
            // clang-format off
            pool.post([&ran]
            {
                ran.fetch_add(1, std::memory_order_relaxed);
            });
            // clang-format on
        }

        std::this_thread::sleep_for(std::chrono::microseconds(round % 3 == 0 ? 1500 : 50));
    }

    ASSERT_TRUE(PoolsTestHelpers::waitFor(ran, kRounds * kPerRound));
    pool.stop();
    EXPECT_EQ(ledger->depth(), 0);
    EXPECT_EQ(pool.threadCount(), 0u);
}

// A job may stop the pool it runs on, and the stop returns without waiting for the thread of that job, which the destructor joins once it retired.
TEST(TaskPool, AJobStopsItsOwnPool)
{
    auto ledger = std::make_shared<WorkLedger>();
    std::atomic<int> stopped{0};
    {
        TaskPool pool(2, kKeepAlive, ledger);
        // clang-format off
        pool.post([&pool, &stopped]
        {
            pool.stop();
            stopped.fetch_add(1, std::memory_order_acq_rel);
        });
        // clang-format on

        ASSERT_TRUE(PoolsTestHelpers::waitFor(stopped, 1));
    }

    EXPECT_EQ(stopped.load(), 1);
}

// The options set the limit of each pool, so a host runs no more workers than it asked for.
TEST(Runtime, OptionsLimitTheTaskAndIoPools)
{
    Runtime runtime(std::vector<std::string>{"varn"}, 1, {2, 3});

    EXPECT_EQ(PoolsTestHelpers::peakConcurrency(runtime.taskPool(), 2, 4), 2);
    EXPECT_EQ(PoolsTestHelpers::peakConcurrency(runtime.ioPool(), 3, 6), 3);
}

// A limit left at zero keeps the default of its pool, a thread per core for the tasks and a high count for the blocking I/O, whose threads mostly wait.
TEST(Runtime, OptionsLeftAtZeroKeepTheDefaultLimits)
{
    Runtime runtime(std::vector<std::string>{"varn"}, 1, {0, 0});
    const auto cores = std::thread::hardware_concurrency();

    EXPECT_EQ(runtime.taskPool().threadLimit(), cores != 0 ? cores : 4u);
    EXPECT_EQ(runtime.ioPool().threadLimit(), 512u);
    EXPECT_EQ(PoolsTestHelpers::peakConcurrency(runtime.ioPool(), 64, 64), 64);
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
    Runtime runtime(std::vector<std::string>{"varn"});
    ASSERT_FALSE(runtime.mainLoop().hasPendingJobs());

    auto resolved = std::make_shared<async::Promise>(runtime);
    resolved->resolve("value");
    EXPECT_FALSE(runtime.mainLoop().hasPendingJobs());

    auto reported = std::make_shared<async::Promise>(runtime);
    reported->rejectReported("The failure was already reported.");
    EXPECT_FALSE(runtime.mainLoop().hasPendingJobs());

    auto rejected = std::make_shared<async::Promise>(runtime);
    rejected->reject("Nobody observed this rejection.");
    EXPECT_TRUE(runtime.mainLoop().hasPendingJobs());
}

} // namespace varn::runtime
