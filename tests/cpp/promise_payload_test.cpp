#include "varn/async/Promise.h"
#include "varn/runtime/Runtime.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace varn::async
{

namespace
{
class PayloadTestHelpers
{
public:
    static lua_Integer global(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const lua_Integer value = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return value;
    }

    // Pumps the way a host run loop would until the script counted every awaiter it waits for, or the budget runs out.
    static void pumpUntil(runtime::Runtime& runtime, const char* name, lua_Integer expected)
    {
        for (int tick = 0; tick < 5000 && global(runtime.luaState(), name) < expected; ++tick)
        {
            runtime.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Publishes a fresh promise to the script under the global name.
    static std::shared_ptr<Promise> publish(runtime::Runtime& runtime, const char* name)
    {
        lua_State* L = runtime.luaState();
        auto promise = std::make_shared<Promise>(runtime);
        Promise::push(L, promise);
        lua_setglobal(L, name);
        return promise;
    }

    // Answers the address of every string in the global list, so two awaiters that hold the same Lua string answer the same address.
    static std::vector<const char*> addresses(lua_State* L, const char* name)
    {
        std::vector<const char*> found;
        lua_getglobal(L, name);
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer index = 1; index <= count; ++index)
        {
            lua_rawgeti(L, -1, index);
            found.push_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
        return found;
    }
};

// Pushes a number and counts its own copies, so a test sees whether a promise copies its push function per awaiter.
class CountingPush
{
public:
    explicit CountingPush(std::shared_ptr<int> copies)
        : copies(std::move(copies))
    {
    }

    CountingPush(const CountingPush& other)
        : copies(other.copies)
    {
        ++*copies;
    }

    CountingPush(CountingPush&& other) noexcept = default;

    void operator()(lua_State* L) const
    {
        lua_pushinteger(L, 7);
    }

private:
    std::shared_ptr<int> copies;
};
} // namespace

constexpr const char* kAwaitTwice = R"lua(
    local async = require("async")

    kept = {}
    finished = 0
    for index = 1, 10 do
        async.spawn(function()
            kept[index] = shared:await()
            finished = finished + 1
        end)
    end

    async.spawn(function()
        while finished < 10 do
            async.sleep(1):await()
        end

        for index = 11, 20 do
            kept[index] = shared:await()
        end

        finished = finished + 10
    end)
)lua";

// A promise resolved with a long string hands the same Lua string to the awaiters that waited for it and to the ones that came after.
TEST(PromisePayload, LongStringIsOneLuaStringForEveryAwaiter)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    ASSERT_EQ(runtime.loadString("require('async')", "=load"), 0);
    auto promise = PayloadTestHelpers::publish(runtime, "shared");

    ASSERT_EQ(runtime.loadString(kAwaitTwice, "=awaiters"), 0);
    promise->resolve(std::string(1024 * 1024, 'p'));
    PayloadTestHelpers::pumpUntil(runtime, "finished", 20);

    lua_State* L = runtime.luaState();
    ASSERT_EQ(PayloadTestHelpers::global(L, "finished"), 20);

    const std::vector<const char*> found = PayloadTestHelpers::addresses(L, "kept");
    ASSERT_EQ(found.size(), 20u);
    for (const char* address : found)
    {
        EXPECT_NE(address, nullptr);
        EXPECT_EQ(address, found.front());
    }
}

// A promise resolved through a push function calls it for every awaiter without ever copying it, so a function that holds a response never copies the response.
TEST(PromisePayload, CustomPushIsNeverCopiedPerAwaiter)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    ASSERT_EQ(runtime.loadString("require('async')", "=load"), 0);
    auto promise = PayloadTestHelpers::publish(runtime, "shared");

    ASSERT_EQ(runtime.loadString(kAwaitTwice, "=awaiters"), 0);
    auto copies = std::make_shared<int>(0);
    promise->resolveCustom(CountingPush(copies));
    PayloadTestHelpers::pumpUntil(runtime, "finished", 20);

    lua_State* L = runtime.luaState();
    ASSERT_EQ(PayloadTestHelpers::global(L, "finished"), 20);
    EXPECT_EQ(*copies, 0);

    lua_getglobal(L, "kept");
    for (lua_Integer index = 1; index <= 20; ++index)
    {
        lua_rawgeti(L, -1, index);
        EXPECT_EQ(lua_tointeger(L, -1), 7);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

// A long string resolved from another thread reaches every awaiter, and a promise whose last owner is another thread releases its shared string on the loop.
TEST(PromisePayload, LongStringCrossesThreads)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});
    ASSERT_EQ(runtime.loadString("require('async')", "=load"), 0);
    auto promise = PayloadTestHelpers::publish(runtime, "shared");
    ASSERT_EQ(runtime.loadString(kAwaitTwice, "=awaiters"), 0);

    // clang-format off
    std::thread resolver([promise]
    {
        promise->resolve(std::string(256 * 1024, 't'));
    });
    // clang-format on

    PayloadTestHelpers::pumpUntil(runtime, "finished", 20);
    resolver.join();
    ASSERT_EQ(PayloadTestHelpers::global(runtime.luaState(), "finished"), 20);

    // The script lets go of the promise, so the thread below drops its last owner while the loop keeps running.
    ASSERT_EQ(runtime.loadString("shared = nil; kept = nil; collectgarbage(); collectgarbage()", "=release"), 0);

    // clang-format off
    std::thread releaser([owned = std::move(promise)]() mutable
    {
        owned.reset();
    });
    // clang-format on

    for (int tick = 0; tick < 50; ++tick)
    {
        runtime.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    releaser.join();
    runtime.poll();
}

} // namespace varn::async
