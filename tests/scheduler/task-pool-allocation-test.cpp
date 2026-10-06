#include "varn/runtime/TaskPool.h"
#include "varn/runtime/WorkLedger.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <thread>

namespace varn::runtime
{

namespace
{

using Clock = std::chrono::steady_clock;

constexpr std::chrono::milliseconds kKeepAlive{10000};

// Counts the allocations of the thread that made it while it lives, which the replaced global allocation functions of this test program report.
class AllocationCounter
{
public:
    AllocationCounter() { active = this; }
    ~AllocationCounter() { active = nullptr; }

    AllocationCounter(const AllocationCounter&) = delete;
    AllocationCounter& operator=(const AllocationCounter&) = delete;

    std::size_t count() const { return allocations; }

    static void record()
    {
        if (active != nullptr)
        {
            ++active->allocations;
        }
    }

private:
    static thread_local AllocationCounter* active;

    std::size_t allocations = 0;
};

thread_local AllocationCounter* AllocationCounter::active = nullptr;

class AllocationTestHelpers
{
public:
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

    // Holds the one worker of the pool in a job until the gate opens, so the jobs posted meanwhile all wait in the queue at once.
    static void holdWorker(TaskPool& pool, std::atomic<bool>& gate, std::atomic<int>& held)
    {
        gate.store(false, std::memory_order_release);

        // clang-format off
        pool.post([&gate, &held]
        {
            held.fetch_add(1, std::memory_order_acq_rel);
            while (!gate.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        // clang-format on
    }
};

} // namespace

// A job whose function keeps its capture inline reaches the queue as it came, and the queue keeps its room, so a backlog that fits allocates nothing.
TEST(TaskPool, PostingAJobThatFitsAllocatesNothingOnceTheBacklogFits)
{
    auto ledger = std::make_shared<WorkLedger>();
    TaskPool pool(1, kKeepAlive, ledger);

    constexpr int kJobs = 200;
    std::atomic<int> ran{0};
    std::atomic<int> held{0};
    std::atomic<bool> gate{false};

    // The first backlog grows the queue to its size, and the second one only reuses it.
    std::size_t allocations = 0;
    for (int round = 1; round <= 2; ++round)
    {
        AllocationTestHelpers::holdWorker(pool, gate, held);
        ASSERT_TRUE(AllocationTestHelpers::waitFor(held, round));

        {
            const AllocationCounter counter;
            for (int index = 0; index < kJobs; ++index)
            {
                // clang-format off
                pool.post([&ran]
                {
                    ran.fetch_add(1, std::memory_order_relaxed);
                });
                // clang-format on
            }

            allocations = counter.count();
        }

        gate.store(true, std::memory_order_release);
        ASSERT_TRUE(AllocationTestHelpers::waitFor(ran, round * kJobs));
    }

    pool.stop();
    EXPECT_EQ(allocations, 0U);
    EXPECT_EQ(ledger->depth(), 0);
}

} // namespace varn::runtime

void* operator new(std::size_t size)
{
    varn::runtime::AllocationCounter::record();
    if (void* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }

    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}
