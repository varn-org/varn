#include "varn/runtime/Runtime.h"
#include "varn/runtime/Scheduler.h"

#include <lua.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace varn::runtime
{

namespace
{

class PumpedLoopHelpers
{
public:
    static bool globalIsSet(Runtime& runtime, const char* name)
    {
        lua_State* L = runtime.luaState();
        const bool set = lua_getglobal(L, name) != LUA_TNIL;
        lua_pop(L, 1);
        return set;
    }
};

} // namespace

// A pumped loop advances only under the polls of its host, so a run that would take the thread until the loop is idle is refused before its chunk runs.
TEST(PumpedLoop, RefusesToRunAChunkToItsEnd)
{
    Scheduler scheduler;
    Runtime runtime(scheduler, std::vector<std::string>{"varn"});

    EXPECT_EQ(runtime.runString("ran = true", "=run"), 1);
    EXPECT_FALSE(PumpedLoopHelpers::globalIsSet(runtime, "ran"));

    EXPECT_EQ(runtime.loadString("loaded = true", "=load"), 0);
    EXPECT_TRUE(PumpedLoopHelpers::globalIsSet(runtime, "loaded"));
}

} // namespace varn::runtime
