#include "varn/dataview/DataView.h"
#include "varn/http/HttpClient.h"
#include "varn/log/Log.h"
#include "varn/runtime/Scheduler.h"

#if defined(VARN_JSON_DRIVER_NLOHMANN)
#include "varn/dataview/DataViewJson.h"
#endif

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace varn::runtime
{

namespace
{

using Clock = std::chrono::steady_clock;

class SchedulerTestHelpers
{
public:
    // Polls the scheduler on this thread until the condition holds, and answers false once a bound that no healthy run reaches passed.
    static bool pollUntil(Scheduler& scheduler, const std::function<bool()>& condition)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (!condition())
        {
            if (Clock::now() >= deadline)
            {
                return false;
            }

            scheduler.poll(std::chrono::milliseconds(5));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }
};

} // namespace

// A job of a pool hands its result back to the loop, which runs the follow-up on the thread that polls, the way a C++ host with no Lua drives the scheduler from its own frames.
TEST(Scheduler, APoolJobContinuesOnTheThreadThatPolls)
{
    Scheduler scheduler;
    const std::thread::id pollingThread = std::this_thread::get_id();
    std::atomic<int> computed{0};
    std::atomic<bool> continuedOnThePoller{false};
    std::atomic<bool> continued{false};

    // clang-format off
    scheduler.taskPool().post([&scheduler, &computed, &continuedOnThePoller, &continued, pollingThread]
    {
        computed.store(42, std::memory_order_release);
        scheduler.mainLoop().post([&continuedOnThePoller, &continued, pollingThread]
        {
            continuedOnThePoller.store(std::this_thread::get_id() == pollingThread, std::memory_order_release);
            continued.store(true, std::memory_order_release);
        });
    });
    // clang-format on

    // clang-format off
    ASSERT_TRUE(SchedulerTestHelpers::pollUntil(scheduler, [&continued] { return continued.load(std::memory_order_acquire); }));
    // clang-format on
    EXPECT_EQ(computed.load(), 42);
    EXPECT_TRUE(continuedOnThePoller.load());
    // clang-format off
    EXPECT_TRUE(SchedulerTestHelpers::pollUntil(scheduler, [&scheduler] { return !scheduler.hasWorkInFlight(); }));
    // clang-format on
}

// A timer fires under the polls once its delay passed, and a poll answers how long the host may sleep until then.
TEST(Scheduler, ATimerFiresUnderThePollsAfterItsDelay)
{
    Scheduler scheduler;
    std::atomic<bool> fired{false};

    const auto armed = Clock::now();
    // clang-format off
    scheduler.mainLoop().postDelayed(30, [&fired]
    {
        fired.store(true, std::memory_order_release);
    });
    // clang-format on

    const EventLoop::PollResult first = scheduler.poll(std::chrono::milliseconds(1));
    EXPECT_TRUE(first.pending);
    EXPECT_GE(first.idleMilliseconds, 0);

    // clang-format off
    ASSERT_TRUE(SchedulerTestHelpers::pollUntil(scheduler, [&fired] { return fired.load(std::memory_order_acquire); }));
    // clang-format on
    EXPECT_GE(Clock::now() - armed, std::chrono::milliseconds(30));
}

// A stop drops what the loop still holds without running it, and every later poll answers that nothing is left.
TEST(Scheduler, AStopDropsTheQueuedJobsWithoutRunningThem)
{
    Scheduler scheduler;
    bool ran = false;

    // clang-format off
    scheduler.mainLoop().post([&ran]
    {
        ran = true;
    });
    scheduler.mainLoop().postDelayed(10, [&ran]
    {
        ran = true;
    });
    // clang-format on
    EXPECT_TRUE(scheduler.hasWorkInFlight());

    scheduler.stop();

    EXPECT_TRUE(scheduler.stopped());
    EXPECT_FALSE(scheduler.hasWorkInFlight());
    EXPECT_FALSE(scheduler.poll());
    EXPECT_FALSE(scheduler.poll(std::chrono::milliseconds(1)).pending);
    EXPECT_FALSE(ran);
}

// The HTTP client runs on the scheduler alone and settles even a request it refuses through the loop, so a C++ host sends requests with no Lua linked.
TEST(Scheduler, TheHttpClientSettlesARefusedRequestOnTheLoop)
{
    Scheduler scheduler;
    http::client::HttpClient client(scheduler);
    std::atomic<bool> completed{false};
    std::optional<http::client::Error> failure;

    http::client::Request request;
    request.url = "ftp://example.invalid/file";

    http::client::HttpClient::Callbacks callbacks;
    // clang-format off
    callbacks.onComplete = [&completed, &failure](const std::optional<http::client::Error>& error)
    {
        failure = error;
        completed.store(true, std::memory_order_release);
    };
    // clang-format on

    const http::client::Handle handle = client.send(std::move(request), std::move(callbacks));
    EXPECT_FALSE(completed.load());

    // clang-format off
    ASSERT_TRUE(SchedulerTestHelpers::pollUntil(scheduler, [&completed] { return completed.load(std::memory_order_acquire); }));
    // clang-format on
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, http::client::ErrorCode::Invalid);
}

// Data views and their tokens belong to the runtime without Lua, so a C++ host shares memory by reference with no Lua linked.
TEST(Scheduler, ADataViewCrossesAsATokenWithoutLua)
{
    const dataview::DataView view = dataview::DataView::allocate(16);
    ASSERT_TRUE(view);
    view.data()[3] = std::byte{7};

    const dataview::DataView acquired = dataview::DataView::acquire(view.token());
    EXPECT_EQ(acquired, view);
    EXPECT_EQ(acquired.data()[3], std::byte{7});

#if defined(VARN_JSON_DRIVER_NLOHMANN)
    const std::vector<dataview::DataView> named = dataview::DataViewJson::acquire(dataview::DataViewJson::write(view));
    ASSERT_EQ(named.size(), 1u);
    EXPECT_EQ(named.front(), view);
#endif
}

// The log pipeline hands every record to the sink of the host, formatted by its backend, with no Lua linked.
TEST(Scheduler, TheLogReachesTheSinkOfTheHost)
{
    std::vector<std::string> records;
    // clang-format off
    log::Log::setSink([&records](log::Level level, std::string_view message)
    {
        if (level == log::Level::Error)
        {
            records.emplace_back(message);
        }
    });
    // clang-format on

    log::Log::error("The scheduler logs without Lua.");
    log::Log::setSink({});

    ASSERT_EQ(records.size(), 1u);
    EXPECT_NE(records.front().find("The scheduler logs without Lua."), std::string::npos);
}

} // namespace varn::runtime
