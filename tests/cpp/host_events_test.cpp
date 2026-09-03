#include "varn/varn.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace
{

// what the lua side handed back, kept where the c callback can reach it without capturing
class Collected
{
public:
    static std::string& lines()
    {
        static std::string storage;
        return storage;
    }

    static std::atomic<int>& count()
    {
        static std::atomic<int> value{0};
        return value;
    }

    static const char* record(const char* argument, void*)
    {
        lines() += argument != nullptr ? argument : "null";
        lines() += "\n";
        count().fetch_add(1, std::memory_order_release);
        return nullptr;
    }
};

} // namespace

// the host reaches lua from another thread while the loop runs, which is the direction a user interface needs
TEST(HostEvents, DeliversFromAnotherThread)
{
    Collected::lines().clear();
    Collected::count().store(0, std::memory_order_release);

    varn_runtime* runtime = varn_runtime_new();
    ASSERT_NE(runtime, nullptr);
    ASSERT_EQ(varn_runtime_register(runtime, "record", &Collected::record, nullptr), 0);

    // the loop is held open so it waits for the events instead of exiting once the chunk returns
    ASSERT_EQ(varn_runtime_retain(runtime), 0);

    std::thread emitter([runtime]
    {
        for (int i = 0; i < 3; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            varn_runtime_emit(runtime, "tap", "{\"index\":1}");
        }

        // wait for lua to have seen all three before letting the loop finish
        while (Collected::count().load(std::memory_order_acquire) < 3)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        varn_runtime_release(runtime);
    });

    const char* script = R"(
        local seen = 0
        host.on("tap", function(payload)
            seen = seen + 1
            host.record("tap " .. tostring(seen) .. " index " .. tostring(payload.index))
        end)
    )";

    EXPECT_EQ(varn_runtime_run_string(runtime, script, "=test"), 0);
    emitter.join();
    varn_runtime_free(runtime);

    EXPECT_EQ(Collected::count().load(std::memory_order_acquire), 3);
    EXPECT_NE(Collected::lines().find("tap 1 index 1"), std::string::npos);
    EXPECT_NE(Collected::lines().find("tap 3 index 1"), std::string::npos);
}

// an event nobody subscribed to is not an error, and one raised inside a handler does not take the loop down
TEST(HostEvents, SurvivesUnknownNamesAndFailingHandlers)
{
    varn_runtime* runtime = varn_runtime_new();
    ASSERT_NE(runtime, nullptr);
    ASSERT_EQ(varn_runtime_retain(runtime), 0);

    std::thread emitter([runtime]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        varn_runtime_emit(runtime, "nobody-listens", "null");
        varn_runtime_emit(runtime, "boom", "null");
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        varn_runtime_release(runtime);
    });

    const char* script = R"(
        host.on("boom", function()
            error("the handler failed")
        end)
    )";

    EXPECT_EQ(varn_runtime_run_string(runtime, script, "=test"), 0);
    emitter.join();
    varn_runtime_free(runtime);
}

// emitting after the runtime was told to stop must not reach the lua state it is tearing down
TEST(HostEvents, IgnoresEmitAfterStop)
{
    varn_runtime* runtime = varn_runtime_new();
    ASSERT_NE(runtime, nullptr);

    EXPECT_EQ(varn_runtime_run_string(runtime, "host.on('late', function() end)", "=test"), 0);

    varn_runtime_stop(runtime);
    EXPECT_EQ(varn_runtime_emit(runtime, "late", "null"), 0);

    varn_runtime_free(runtime);
}
