#include "varn/runtime/EventLoop.h"
#include "varn/runtime/HostWork.h"
#include "varn/runtime/Scheduler.h"
#include "varn/runtime/TaskPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace varn::runtime
{

namespace
{

// Where a job of a pool and the work it hands back to the loop ran, as the host reads it after its polls.
struct Placement
{
    std::atomic<std::thread::id> job{};
    std::atomic<std::thread::id> followUp{};
    std::atomic<bool> done{false};
};

class PumpedLoopHelpers
{
public:
    // Posts a job to the task pool that records its thread and posts its follow-up to the loop, as a decode that hands its result to Lua does.
    static void postRecordingJob(Scheduler& scheduler, Placement& placement)
    {
        EventLoop& loop = scheduler.mainLoop();
        // clang-format off
        scheduler.taskPool().post([&placement, &loop]
        {
            placement.job.store(std::this_thread::get_id());
            loop.post([&placement]
            {
                placement.followUp.store(std::this_thread::get_id());
                placement.done.store(true);
            });
        });
        // clang-format on
    }

    // Polls the way the run loop of a host does, once a frame, until the follow-up ran or the frames run out.
    static bool pollUntilDone(Scheduler& scheduler, const Placement& placement)
    {
        for (int frame = 0; frame < 5000 && !placement.done.load(); ++frame)
        {
            scheduler.poll(std::chrono::milliseconds(4));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return placement.done.load();
    }
};

} // namespace

// Work that the host completes outside the queue of the loop keeps both polls pending until the host ends it.
TEST(PumpedLoop, WorkOfTheHostKeepsThePollPending)
{
    Scheduler scheduler;
    EXPECT_FALSE(scheduler.poll());

    HostWork::begin();
    EXPECT_TRUE(scheduler.poll());
    EXPECT_TRUE(scheduler.poll(std::chrono::milliseconds(1)).pending);

    HostWork::end();
    EXPECT_FALSE(scheduler.poll());
    EXPECT_FALSE(scheduler.poll(std::chrono::milliseconds(1)).pending);
}

#if defined(VARN_THREADS)

// With threads the pools run their jobs on workers of their own, so the thread that pumps only runs what the jobs hand back to the loop.
TEST(PumpedLoop, RunsPoolJobsOnWorkersAndTheirFollowUpsOnThePump)
{
    Scheduler scheduler;
    scheduler.poll(std::chrono::milliseconds(1));

    Placement placement;
    PumpedLoopHelpers::postRecordingJob(scheduler, placement);

    ASSERT_TRUE(PumpedLoopHelpers::pollUntilDone(scheduler, placement));
    EXPECT_NE(placement.job.load(), std::this_thread::get_id());
    EXPECT_EQ(placement.followUp.load(), std::this_thread::get_id());
    EXPECT_GE(scheduler.taskPool().threadCount(), 1u);
}

// A worker that posts to the loop wakes a host that sleeps in its own run loop, while a post of the pumping thread itself wakes no one.
TEST(PumpedLoop, AWorkerThatPostsWakesTheHost)
{
    Scheduler scheduler;
    scheduler.poll(std::chrono::milliseconds(1));
    std::atomic<int> wakes{0};
    // clang-format off
    scheduler.mainLoop().setWakeHandler([&wakes]
    {
        wakes.fetch_add(1);
    });
    // clang-format on

    Placement placement;
    PumpedLoopHelpers::postRecordingJob(scheduler, placement);
    for (int tick = 0; tick < 5000 && wakes.load() == 0; ++tick)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_GE(wakes.load(), 1);
    ASSERT_TRUE(PumpedLoopHelpers::pollUntilDone(scheduler, placement));
    scheduler.mainLoop().setWakeHandler(nullptr);
}

// Many jobs post their follow-ups from every worker while the host pumps, and each one runs once on the pump.
TEST(PumpedLoop, FollowUpsOfManyWorkersRunOnceOnThePump)
{
    Scheduler scheduler;
    scheduler.poll(std::chrono::milliseconds(1));

    constexpr int kJobs = 2000;
    std::atomic<int> ran{0};
    std::atomic<int> offPump{0};
    const std::thread::id pump = std::this_thread::get_id();
    EventLoop& loop = scheduler.mainLoop();
    for (int index = 0; index < kJobs; ++index)
    {
        TaskPool& pool = index % 2 == 0 ? scheduler.taskPool() : scheduler.ioPool();
        // clang-format off
        pool.post([&loop, &ran, &offPump, pump]
        {
            loop.post([&ran, &offPump, pump]
            {
                if (std::this_thread::get_id() != pump)
                {
                    offPump.fetch_add(1);
                }

                ran.fetch_add(1);
            });
        });
        // clang-format on
    }

    for (int frame = 0; frame < 10000 && ran.load() < kJobs; ++frame)
    {
        scheduler.poll(std::chrono::milliseconds(4));
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    EXPECT_EQ(ran.load(), kJobs);
    EXPECT_EQ(offPump.load(), 0);
}

#else

// Without threads the pools keep no worker and the pump runs their jobs, so everything runs on the thread that pumps.
TEST(PumpedLoop, RunsPoolJobsInsideThePumpWithoutThreads)
{
    Scheduler scheduler;
    scheduler.poll(std::chrono::milliseconds(1));

    Placement placement;
    PumpedLoopHelpers::postRecordingJob(scheduler, placement);
    EXPECT_FALSE(placement.done.load());

    ASSERT_TRUE(PumpedLoopHelpers::pollUntilDone(scheduler, placement));
    EXPECT_EQ(placement.job.load(), std::this_thread::get_id());
    EXPECT_EQ(placement.followUp.load(), std::this_thread::get_id());
    EXPECT_EQ(scheduler.taskPool().threadCount(), 0u);
}

// A poll without a budget, and one whose budget reaches the end of the clock, run a pool job and its follow-up in the same call.
TEST(PumpedLoop, APollRunsEveryTurnOfThePoolsAndTheLoop)
{
    Scheduler scheduler;

    Placement first;
    PumpedLoopHelpers::postRecordingJob(scheduler, first);
    EXPECT_FALSE(scheduler.poll());
    EXPECT_TRUE(first.done.load());

    Placement second;
    PumpedLoopHelpers::postRecordingJob(scheduler, second);
    const EventLoop::PollResult result = scheduler.poll(std::chrono::nanoseconds::max());
    EXPECT_TRUE(result.ran);
    EXPECT_FALSE(result.pending);
    EXPECT_TRUE(second.done.load());
}

#endif

} // namespace varn::runtime
