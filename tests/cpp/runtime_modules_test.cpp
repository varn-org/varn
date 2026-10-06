#include "varn/runtime/Runtime.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <string>
#include <vector>

namespace varn::runtime
{

namespace
{
class ModuleTestHelpers
{
public:
    static inline int opened = 0;

    // Answers a module with one function, and counts how often the runtime opened it.
    static int openAnswers(lua_State* L)
    {
        ++opened;
        lua_newtable(L);
        lua_pushcfunction(L, &ModuleTestHelpers::answer);
        lua_setfield(L, -2, "answer");
        return 1;
    }

    static int answer(lua_State* L)
    {
        lua_pushinteger(L, 42);
        return 1;
    }
};
} // namespace

// A module a host adds loads on its first `require`, once, like any module of the runtime.
TEST(RuntimeModules, AHostModuleLoadsOnItsFirstRequire)
{
    ModuleTestHelpers::opened = 0;
    Runtime runtime(std::vector<std::string>{"varn"});
    ASSERT_TRUE(runtime.addModule("host.answers", &ModuleTestHelpers::openAnswers));
    EXPECT_EQ(ModuleTestHelpers::opened, 0);

    EXPECT_EQ(runtime.runString("assert(require('host.answers').answer() == 42) assert(require('host.answers') == require('host.answers'))", "=modules"), 0);
    EXPECT_EQ(ModuleTestHelpers::opened, 1);
}

// A name the runtime or an earlier call already answers is refused, so a host never replaces a module by accident.
TEST(RuntimeModules, ANameAlreadyTakenIsRefused)
{
    Runtime runtime(std::vector<std::string>{"varn"});

    EXPECT_FALSE(runtime.addModule("fs", &ModuleTestHelpers::openAnswers));
    EXPECT_FALSE(runtime.addModule("async", &ModuleTestHelpers::openAnswers));
    ASSERT_TRUE(runtime.addModule("host.answers", &ModuleTestHelpers::openAnswers));
    EXPECT_FALSE(runtime.addModule("host.answers", &ModuleTestHelpers::openAnswers));
    EXPECT_FALSE(runtime.addModule("", &ModuleTestHelpers::openAnswers));
    EXPECT_FALSE(runtime.addModule("host.nothing", nullptr));

    EXPECT_EQ(runtime.runString("assert(require('fs').stat ~= nil)", "=kept"), 0);
}

// A symbol keeps the first address given for its name, accepts that address again and refuses any other one.
TEST(RuntimeSymbols, ASymbolKeepsItsFirstAddress)
{
    Runtime runtime(std::vector<std::string>{"varn"});
    int first = 0;
    int second = 0;

    ASSERT_TRUE(runtime.addSymbol("host_value", &first));
    EXPECT_TRUE(runtime.addSymbol("host_value", &first));
    EXPECT_FALSE(runtime.addSymbol("host_value", &second));
    EXPECT_FALSE(runtime.addSymbol("", &first));
    EXPECT_FALSE(runtime.addSymbol("host_nothing", nullptr));
}

} // namespace varn::runtime
