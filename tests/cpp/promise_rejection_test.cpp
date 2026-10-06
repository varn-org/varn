#include "varn/runtime/Runtime.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <chrono>
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
            runtime.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};
} // namespace

// Promises the I/O pool rejects while the loop thread awaits some of them are reported exactly when no one observed them, whichever thread settles first.
TEST(PromiseRejections, PoolRejectionsAreReportedOnlyWhenUnobserved)
{
    runtime::Runtime runtime(std::vector<std::string>{"varn"});

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

} // namespace varn::async
