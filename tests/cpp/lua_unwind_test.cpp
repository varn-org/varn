#include <gtest/gtest.h>
#include <lua.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

namespace
{
class LuaUnwindTestHelpers
{
public:
    // Counts the destructors that ran, so a test can tell whether an unwind reached a C++ frame.
    struct Tracker
    {
        static inline int destroyed = 0;

        ~Tracker() { ++destroyed; }
    };

    static std::unique_ptr<lua_State, decltype(&lua_close)> open()
    {
        std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
        luaL_openlibs(state.get());
        return state;
    }

    // Raises a Lua error while a destructible local is alive.
    static int failWithTracker(lua_State* L)
    {
        const Tracker tracker;
        const std::string message = "Raised with a live destructor";
        lua_pushlstring(L, message.data(), message.size());
        return lua_error(L);
    }

    static int callContinuation(lua_State* L, int status, lua_KContext ctx)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(ctx));
        lua_pushboolean(L, status == LUA_YIELD);
        return lua_gettop(L) - 1;
    }

    // Calls its argument through a continuation, so a yield inside it leaves this frame and resumes in the continuation.
    static int callThrough(lua_State* L)
    {
        lua_pushvalue(L, 1);
        lua_callk(L, 0, 1, 7, &callContinuation);
        return callContinuation(L, LUA_OK, 7);
    }

    static int yieldContinuation(lua_State* L, int status, lua_KContext ctx)
    {
        lua_pushinteger(L, lua_tointeger(L, -1) + static_cast<lua_Integer>(ctx));
        lua_pushboolean(L, status == LUA_YIELD);
        return 2;
    }

    // Yields its argument and adds its context to the value it is resumed with.
    static int yieldThrough(lua_State* L)
    {
        return lua_yieldk(L, 1, 100, &yieldContinuation);
    }

    // Answers the best cost of one yield and its resume over several batches, in nanoseconds.
    static double bestYieldNanoseconds(lua_State* L, int batches, int count)
    {
        lua_State* thread = lua_newthread(L);
        if (luaL_loadstring(thread, "while true do coroutine.yield() end") != LUA_OK)
        {
            return -1;
        }

        double best = 1e12;
        for (int batch = 0; batch < batches; ++batch)
        {
            const auto started = std::chrono::steady_clock::now();
            for (int i = 0; i < count; ++i)
            {
                int results = 0;
                if (lua_resume(thread, L, 0, &results) != LUA_YIELD)
                {
                    return -1;
                }
            }

            const std::chrono::duration<double, std::nano> elapsed = std::chrono::steady_clock::now() - started;
            best = std::min(best, elapsed.count() / count);
        }

        lua_pop(L, 1);
        return best;
    }
};
} // namespace

// A Lua error stays a C++ exception, so it runs the destructors of the C++ frames it leaves.
TEST(LuaUnwind, ErrorRunsTheDestructorsOfTheFramesItLeaves)
{
    auto state = LuaUnwindTestHelpers::open();
    lua_State* L = state.get();
    const int before = LuaUnwindTestHelpers::Tracker::destroyed;

    lua_pushcfunction(L, &LuaUnwindTestHelpers::failWithTracker);
    ASSERT_EQ(lua_pcall(L, 0, 0, 0), LUA_ERRRUN);

    EXPECT_STREQ(lua_tostring(L, -1), "Raised with a live destructor");
    EXPECT_EQ(LuaUnwindTestHelpers::Tracker::destroyed, before + 1);
}

// A yield below a call with a continuation leaves the C frame and resumes in its continuation with the context it gave.
TEST(LuaUnwind, YieldBelowACallResumesInItsContinuation)
{
    auto state = LuaUnwindTestHelpers::open();
    lua_State* L = state.get();
    lua_register(L, "callThrough", &LuaUnwindTestHelpers::callThrough);

    lua_State* thread = lua_newthread(L);
    ASSERT_EQ(luaL_loadstring(thread, "return callThrough(function() return coroutine.yield('paused') + 1 end)"), LUA_OK);

    int results = 0;
    ASSERT_EQ(lua_resume(thread, L, 0, &results), LUA_YIELD);
    ASSERT_EQ(results, 1);
    EXPECT_STREQ(lua_tostring(thread, -1), "paused");
    lua_pop(thread, results);

    lua_pushinteger(thread, 41);
    ASSERT_EQ(lua_resume(thread, L, 1, &results), LUA_OK);
    ASSERT_EQ(results, 3);
    EXPECT_EQ(lua_tointeger(thread, -3), 42);
    EXPECT_EQ(lua_tointeger(thread, -2), 7);
    EXPECT_TRUE(lua_toboolean(thread, -1));
}

// A C function that yields with a continuation answers through it once resumed.
TEST(LuaUnwind, YieldFromACFunctionResumesInItsContinuation)
{
    auto state = LuaUnwindTestHelpers::open();
    lua_State* L = state.get();
    lua_register(L, "yieldThrough", &LuaUnwindTestHelpers::yieldThrough);

    lua_State* thread = lua_newthread(L);
    ASSERT_EQ(luaL_loadstring(thread, "local value, resumed = yieldThrough(5) return value, resumed"), LUA_OK);

    int results = 0;
    ASSERT_EQ(lua_resume(thread, L, 0, &results), LUA_YIELD);
    ASSERT_EQ(results, 1);
    EXPECT_EQ(lua_tointeger(thread, -1), 5);
    lua_pop(thread, results);

    lua_pushinteger(thread, 20);
    ASSERT_EQ(lua_resume(thread, L, 1, &results), LUA_OK);
    ASSERT_EQ(results, 2);
    EXPECT_EQ(lua_tointeger(thread, -2), 120);
    EXPECT_TRUE(lua_toboolean(thread, -1));
}

// An error raised after a yield reaches the resume as an error, with the C++ destructors below it run.
TEST(LuaUnwind, ErrorAfterAYieldReachesTheResume)
{
    auto state = LuaUnwindTestHelpers::open();
    lua_State* L = state.get();
    lua_register(L, "failWithTracker", &LuaUnwindTestHelpers::failWithTracker);
    const int before = LuaUnwindTestHelpers::Tracker::destroyed;

    lua_State* thread = lua_newthread(L);
    ASSERT_EQ(luaL_loadstring(thread, "coroutine.yield() coroutine.yield() failWithTracker()"), LUA_OK);

    int results = 0;
    ASSERT_EQ(lua_resume(thread, L, 0, &results), LUA_YIELD);
    ASSERT_EQ(lua_resume(thread, L, 0, &results), LUA_YIELD);
    ASSERT_EQ(lua_resume(thread, L, 0, &results), LUA_ERRRUN);

    EXPECT_STREQ(lua_tostring(thread, -1), "Raised with a live destructor");
    EXPECT_EQ(LuaUnwindTestHelpers::Tracker::destroyed, before + 1);
}

// A yield jumps straight back to its resume, so it costs well under a microsecond once optimized.
TEST(LuaUnwind, YieldAndResumeCostWellUnderAMicrosecond)
{
#if !defined(NDEBUG) || defined(VARN_TESTS_SANITIZED) || defined(_MSC_VER)
    GTEST_SKIP() << "The cost of a yield is measured only in an optimized build without sanitizers, and MSVC yields through exceptions by design.";
#endif

    auto state = LuaUnwindTestHelpers::open();
    const double nanoseconds = LuaUnwindTestHelpers::bestYieldNanoseconds(state.get(), 5, 100000);

    ASSERT_GT(nanoseconds, 0.0);
    EXPECT_LT(nanoseconds, 500.0);
}
