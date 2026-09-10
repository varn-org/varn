#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"
#include "varn/runtime/WorkLedger.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace varn::runtime
{

namespace
{
class ConcurrencyTestHelpers
{
public:
    static void waitForDrain(const std::shared_ptr<WorkLedger>& ledger)
    {
        while (ledger->depth() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};
} // namespace

TEST(TaskPool, RunsEveryPostedJobAndBalancesTheLedger)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(4, ledger);
    pool.start();

    std::atomic<int> ran{0};
    constexpr int kJobs = 5000;
    for (int i = 0; i < kJobs; ++i)
    {
        // clang-format off
        pool.post([&ran]
        {
            ran.fetch_add(1, std::memory_order_relaxed);
        });
        // clang-format on
    }

    ConcurrencyTestHelpers::waitForDrain(ledger);
    pool.stop();

    EXPECT_EQ(ran.load(), kJobs);
    EXPECT_EQ(ledger->depth(), 0);
}

TEST(TaskPool, ConcurrentProducersHaveEveryJobRun)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(6, ledger);
    pool.start();

    std::atomic<int> ran{0};
    constexpr int kProducers = 8;
    constexpr int kPerProducer = 2000;

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p)
    {
        // clang-format off
        producers.emplace_back([&pool, &ran]
        {
            for (int i = 0; i < kPerProducer; ++i)
            {
                pool.post([&ran] { ran.fetch_add(1, std::memory_order_relaxed); });
            }
        });
        // clang-format on
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    ConcurrencyTestHelpers::waitForDrain(ledger);
    pool.stop();

    EXPECT_EQ(ran.load(), kProducers * kPerProducer);
    EXPECT_EQ(ledger->depth(), 0);
}

TEST(TaskPool, AThrowingJobDoesNotCrashAndReleasesItsLedgerEntry)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(2, ledger);
    pool.start();

    std::atomic<int> ranAfter{0};
    // clang-format off
    pool.post([]
    {
        throw std::runtime_error("Boom");
    });
    pool.post([&ranAfter]
    {
        ranAfter.fetch_add(1, std::memory_order_relaxed);
    });
    // clang-format on

    ConcurrencyTestHelpers::waitForDrain(ledger);
    pool.stop();

    EXPECT_EQ(ranAfter.load(), 1);
    EXPECT_EQ(ledger->depth(), 0);
}

TEST(EventLoop, RunsImmediateAndDelayedJobsThenExitsWhenIdle)
{
    auto ledger = std::make_shared<WorkLedger>();
    EventLoop loop(ledger);

    std::atomic<int> ran{0};
    // clang-format off
    loop.setIdleExitPredicate([&ledger]
    {
        return ledger->depth() == 0;
    });
    loop.post([&ran]
    {
        ran.fetch_add(1, std::memory_order_relaxed);
    });
    loop.postDelayed(2, [&ran]
    {
        ran.fetch_add(1, std::memory_order_relaxed);
    });
    // clang-format on

    // clang-format off
    std::thread runner([&loop] { loop.run(); });
    // clang-format on
    runner.join();

    EXPECT_EQ(ran.load(), 2);
    EXPECT_EQ(ledger->depth(), 0);
}

// A cancelled timer never runs and gives its ledger entry back, so the loop exits at once instead of waiting for a deadline nobody needs.
TEST(EventLoop, ACancelledTimerNeverRunsAndLetsTheLoopExit)
{
    auto ledger = std::make_shared<WorkLedger>();
    EventLoop loop(ledger);

    std::atomic<int> ran{0};
    // clang-format off
    loop.setIdleExitPredicate([&ledger]
    {
        return ledger->depth() == 0;
    });
    const auto kept = loop.postDelayed(2, [&ran]
    {
        ran.fetch_add(1, std::memory_order_relaxed);
    });
    const auto cancelled = loop.postDelayed(60000, [&ran]
    {
        ran.fetch_add(10, std::memory_order_relaxed);
    });
    // clang-format on

    EXPECT_NE(kept, cancelled);
    EXPECT_EQ(ledger->depth(), 2);
    EXPECT_TRUE(loop.cancelTimer(cancelled));
    EXPECT_FALSE(loop.cancelTimer(cancelled));
    EXPECT_EQ(ledger->depth(), 1);

    const auto started = std::chrono::steady_clock::now();
    // clang-format off
    std::thread runner([&loop] { loop.run(); });
    // clang-format on
    runner.join();

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count(), 1000);
    EXPECT_EQ(ran.load(), 1);
    EXPECT_FALSE(loop.cancelTimer(kept));
    EXPECT_EQ(ledger->depth(), 0);
    EXPECT_FALSE(loop.hasPendingTimers());
}

TEST(EventLoop, RunsJobsPostedFromAnotherThread)
{
    auto ledger = std::make_shared<WorkLedger>();
    EventLoop loop(ledger);

    std::atomic<int> ran{0};
    // clang-format off
    loop.setIdleExitPredicate([&ledger]
    {
        return ledger->depth() == 0;
    });
    // clang-format on

    // Hold the loop open so it cannot idle out before the cross-thread posts arrive.
    ledger->enter();
    // clang-format off
    std::thread runner([&loop] { loop.run(); });
    // clang-format on

    constexpr int kJobs = 2000;
    for (int i = 0; i < kJobs; ++i)
    {
        // clang-format off
        loop.post([&ran]
        {
            ran.fetch_add(1, std::memory_order_relaxed);
        });
        // clang-format on
    }

    ledger->leave();
    loop.wake();
    runner.join();

    EXPECT_EQ(ran.load(), kJobs);
    EXPECT_EQ(ledger->depth(), 0);
}

TEST(Runtime, FirstIoPoolUseRacesACrossThreadStopSafely)
{
    // The I/O pool is created lazily on one thread while `stop()` reaches it from another, so the attempt is repeated to land inside that narrow window.
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        Runtime runtime(std::vector<std::string>{"varn"});

        std::atomic<bool> ready{false};
        // clang-format off
        std::thread user([&runtime, &ready]
        {
            ready.store(true, std::memory_order_release);
            runtime.ioPool().post([] {});
        });
        // clang-format on

        while (!ready.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        runtime.stop();
        user.join();
    }
}

} // namespace varn::runtime
