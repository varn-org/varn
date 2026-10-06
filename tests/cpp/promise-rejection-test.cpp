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
class RejectionTestHelpers
{
public:
    static lua_Integer global(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const lua_Integer value = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return value;
    }

    // Pumps the way a host run loop would until the script counted every outcome it waits for, or the budget runs out.
    static void pumpUntil(runtime::Runtime& runtime, const char* name, lua_Integer expected)
    {
        for (int tick = 0; tick < 5000 && global(runtime.luaState(), name) < expected; ++tick)
        {
            runtime.scheduler().poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    // Answers whether the registry still holds the table that the global of the name held when the test stored it.
    static bool registryHolds(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const void* wanted = lua_topointer(L, -1);
        lua_pop(L, 1);

        bool found = false;
        lua_pushnil(L);
        while (lua_next(L, LUA_REGISTRYINDEX) != 0)
        {
            found = found || (lua_istable(L, -1) && lua_topointer(L, -1) == wanted);
            lua_pop(L, 1);
        }

        return found;
    }
};
} // namespace

// A rejection with a failure table hands the table to every awaiter and to catch handlers next to its text, and one the failure handler already received is never reported again.
TEST(PromiseRejections, AwaitersReceiveTheFailureOfARejection)
{
    runtime::Scheduler scheduler;
    runtime::Runtime runtime(scheduler, std::vector<std::string>{"varn"});
    lua_State* L = runtime.luaState();
    auto promise = std::make_shared<Promise>(runtime);
    Promise::push(L, promise);
    lua_setglobal(L, "pending");

    const char* script = R"lua(
        local async = require("async")
        reported = 0
        answered = 0
        async.onFailure(function() reported = reported + 1 end)

        local function check(err, failure)
            assert(err == "The record is damaged.", "The awaiter receives the text of the error.")
            assert(failure.code == "chunkHashMismatch" and failure.error == err, "The awaiter receives the failure table.")
            answered = answered + 1
        end
        async.spawn(function()
            local value, err, failure = pending:await()
            assert(value == nil)
            check(err, failure)
        end)
        pending:catch(check)
    )lua";
    ASSERT_EQ(luaL_dostring(L, script), LUA_OK) << lua_tostring(L, -1);

    lua_createtable(L, 0, 2);
    lua_pushliteral(L, "The record is damaged.");
    lua_setfield(L, -2, "error");
    lua_pushliteral(L, "chunkHashMismatch");
    lua_setfield(L, -2, "code");
    promise->rejectFailure(L, "The promise failed.", true);
    RejectionTestHelpers::pumpUntil(runtime, "answered", 2);

    // An await after the promise settled answers in place with the same failure.
    ASSERT_EQ(luaL_dostring(L, R"lua(require("async").spawn(function() local _, err, failure = pending:await() if failure and failure.code == "chunkHashMismatch" then answered = answered + 1 end end))lua"), LUA_OK) << lua_tostring(L, -1);
    RejectionTestHelpers::pumpUntil(runtime, "answered", 3);
    EXPECT_EQ(RejectionTestHelpers::global(L, "answered"), 3);
    EXPECT_EQ(RejectionTestHelpers::global(L, "reported"), 0);
}

// Promises the I/O pool rejects while the loop thread awaits some of them are reported exactly when no one observed them, whichever thread settles first.
TEST(PromiseRejections, PoolRejectionsAreReportedOnlyWhenUnobserved)
{
    runtime::Scheduler scheduler;
    runtime::Runtime runtime(scheduler, std::vector<std::string>{"varn"});

    const char* script = R"lua(
        local async = require("async")
        local fs = require("fs")

        reported = 0
        answered = 0
        async.onFailure(function(err, traceback, frames, kind)
            assert(kind == "rejection" and traceback == nil and frames == nil, "Only native rejections should be reported.")
            reported = reported + 1
        end)

        for index = 1, 64 do
            fs.readFile("/varn/missing/" .. index)
            async.spawn(function()
                local _, err = fs.readFile("/varn/missing/awaited/" .. index):await()
                assert(err ~= nil, "A missing file should reject.")
                answered = answered + 1
            end)
        end
    )lua";

    ASSERT_EQ(runtime.loadString(script, "=rejections"), 0);
    RejectionTestHelpers::pumpUntil(runtime, "answered", 64);
    RejectionTestHelpers::pumpUntil(runtime, "reported", 64);

    lua_State* L = runtime.luaState();
    EXPECT_EQ(RejectionTestHelpers::global(L, "answered"), 64);
    EXPECT_EQ(RejectionTestHelpers::global(L, "reported"), 64);
}

// The last owner of a promise may be a pool thread, so the failure it kept leaves the registry on the loop and never on that thread.
TEST(PromiseRejections, AFailureDroppedOnAnotherThreadIsReleasedOnTheLoop)
{
    runtime::Scheduler scheduler;
    runtime::Runtime runtime(scheduler, std::vector<std::string>{"varn"});
    lua_State* L = runtime.luaState();

    auto promise = std::make_shared<Promise>(runtime);
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "The record is damaged.");
    lua_setfield(L, -2, "error");
    lua_pushvalue(L, -1);
    lua_setglobal(L, "failure");
    promise->rejectFailure(L, "The record is damaged.", true);
    ASSERT_TRUE(RejectionTestHelpers::registryHolds(L, "failure"));

    // clang-format off
    std::thread dropper([held = std::move(promise)]() mutable
    {
        held.reset();
    });
    // clang-format on
    dropper.join();
    EXPECT_TRUE(RejectionTestHelpers::registryHolds(L, "failure"));

    scheduler.poll();
    EXPECT_FALSE(RejectionTestHelpers::registryHolds(L, "failure"));
}

} // namespace varn::async
