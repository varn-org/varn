#include "varn/runtime/WorkLedger.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace varn::runtime
{

TEST(WorkLedger, EnterAndLeaveTrackDepth)
{
    WorkLedger ledger;
    EXPECT_EQ(ledger.depth(), 0);

    ledger.enter();
    ledger.enter();
    EXPECT_EQ(ledger.depth(), 2);

    ledger.leave();
    EXPECT_EQ(ledger.depth(), 1);

    ledger.leave();
    EXPECT_EQ(ledger.depth(), 0);
}

// Only the last entry leaving can let an idle loop exit, so the ledger notifies then and on no other change.
TEST(WorkLedger, NotifiesOnlyWhenTheLastEntryLeaves)
{
    WorkLedger ledger;
    std::atomic<int> notifications{0};
    // clang-format off
    ledger.setNotify([&notifications]
    {
        notifications.fetch_add(1, std::memory_order_relaxed);
    });
    // clang-format on

    ledger.enter();
    ledger.enter();
    EXPECT_EQ(notifications.load(), 0);

    ledger.leave();
    EXPECT_EQ(notifications.load(), 0);

    ledger.leave();
    EXPECT_EQ(notifications.load(), 1);
}

TEST(WorkLedger, ConcurrentEnterLeaveBalancesToZero)
{
    WorkLedger ledger;
    std::atomic<int> notifications{0};
    // clang-format off
    ledger.setNotify([&notifications]
    {
        notifications.fetch_add(1, std::memory_order_relaxed);
    });
    // clang-format on

    constexpr int kThreads = 16;
    constexpr int kOpsPerThread = 20000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        // clang-format off
        threads.emplace_back([&ledger]
        {
            for (int i = 0; i < kOpsPerThread; ++i)
            {
                ledger.enter();
                ledger.leave();
            }
        });
        // clang-format on
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(ledger.depth(), 0);
    EXPECT_GE(notifications.load(), 1);
    EXPECT_LE(notifications.load(), kThreads * kOpsPerThread);
}

} // namespace varn::runtime
