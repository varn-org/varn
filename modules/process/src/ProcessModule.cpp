#include "varn/process/ProcessModule.h"
#include "varn/async/AsyncTask.h"
#include "varn/async/Promise.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/process/ProcessRunner.h"
#include "varn/runtime/Runtime.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace varn::process
{

using varn::async::Promise;
using varn::runtime::Runtime;

namespace
{
class ProcessModuleHelpers
{
public:
    // Reads the option "timeoutMs" as a whole number of milliseconds, so a fraction or a string never turns the deadline off unnoticed.
    static long long readTimeout(lua_State* L, int index)
    {
        if (lua_isnoneornil(L, index))
        {
            return 0;
        }

        luaL_checktype(L, index, LUA_TTABLE);
        lua_getfield(L, index, "timeoutMs");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return 0;
        }

        int isInteger = 0;
        const lua_Integer timeoutMs = lua_type(L, -1) == LUA_TNUMBER ? lua_tointegerx(L, -1, &isInteger) : 0;
        if (isInteger == 0)
        {
            luaL_error(L, "The option \"timeoutMs\" must be a whole number of milliseconds.");
        }

        lua_pop(L, 1);
        if (timeoutMs < 0)
        {
            luaL_error(L, "A timeout must not be negative.");
        }

        return static_cast<long long>(timeoutMs);
    }

    static void pushArgvTable(lua_State* L, Runtime& rt)
    {
        // Builds a 1-based array of the script arguments only.
        const auto& args = rt.args();
        const std::size_t base = rt.scriptArgIndex();

        lua_createtable(L, args.size() > base ? static_cast<int>(args.size() - base) : 0, 0);
        for (std::size_t i = base; i < args.size(); ++i)
        {
            lua_pushlstring(L, args[i].data(), args[i].size());
            lua_rawseti(L, -2, static_cast<lua_Integer>(i - base + 1));
        }
    }
};
} // namespace

Runtime& ProcessModule::luaRuntime(lua_State* L)
{
    return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
}

int ProcessModule::luaExec(lua_State* L)
{
    std::string command = varn::lua::LuaHelpers::checkString(L, 1);

    if (command.find('\0') != std::string::npos)
    {
        return luaL_error(L, "A command must not contain a null byte.");
    }

    const long long timeoutMs = ProcessModuleHelpers::readTimeout(L, 2);
    auto& rt = luaRuntime(L);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.scheduler().ioPool(), [command = std::move(command), timeoutMs](Promise& promise)
    {
        const ProcessResult result = ProcessRunner::exec(command, timeoutMs);
        if (result.timedOut)
        {
            promise.reject("The command exceeded its timeout of " + std::to_string(timeoutMs) + " ms and was killed.");
            return;
        }

        promise.resolveCustom([result](lua_State* lua)
        {
            lua_createtable(lua, 0, 3);
            lua_pushlstring(lua, result.stdoutData.data(), result.stdoutData.size());
            lua_setfield(lua, -2, "stdout");
            lua_pushlstring(lua, result.stderrData.data(), result.stderrData.size());
            lua_setfield(lua, -2, "stderr");
            lua_pushinteger(lua, static_cast<lua_Integer>(result.code));
            lua_setfield(lua, -2, "code");
        });
    });
    // clang-format on
}

int ProcessModule::luaGetenv(lua_State* L)
{
    const std::string name = varn::lua::LuaHelpers::checkString(L, 1);

    std::optional<std::string> value;
    try
    {
        value = ProcessRunner::getenv(name);
    }
    catch (const std::runtime_error& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }

    if (value)
    {
        lua_pushlstring(L, value->data(), value->size());
        return 1;
    }

    // Falls back to the optional default argument.
    lua_settop(L, 2);
    return 1;
}

int ProcessModule::luaEnvironment(lua_State* L)
{
    std::vector<std::pair<std::string, std::string>> entries;
    try
    {
        entries = ProcessRunner::environment();
    }
    catch (const std::runtime_error& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }

    lua_createtable(L, 0, static_cast<int>(entries.size()));
    for (const auto& entry : entries)
    {
        lua_pushlstring(L, entry.second.data(), entry.second.size());
        lua_setfield(L, -2, entry.first.c_str());
    }

    return 1;
}

int ProcessModule::luaCwd(lua_State* L)
{
    std::string dir;
    try
    {
        dir = ProcessRunner::cwd();
    }
    catch (const std::runtime_error& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }

    lua_pushlstring(L, dir.data(), dir.size());
    return 1;
}

int ProcessModule::luaOpen(lua_State* L)
{
    auto& rt = luaRuntime(L);

    lua_newtable(L);

    lua_pushcfunction(L, &ProcessModule::luaExec);
    lua_setfield(L, -2, "exec");

    lua_pushcfunction(L, &ProcessModule::luaGetenv);
    lua_setfield(L, -2, "getenv");

    lua_pushcfunction(L, &ProcessModule::luaCwd);
    lua_setfield(L, -2, "cwd");

    lua_pushcfunction(L, &ProcessModule::luaEnvironment);
    lua_setfield(L, -2, "environment");

    ProcessModuleHelpers::pushArgvTable(L, rt);
    lua_setfield(L, -2, "argv");

    lua_pushboolean(L, ProcessRunner::available());
    lua_setfield(L, -2, "available");

    return 1;
}

void ProcessModule::install(lua_State* L)
{
    luaL_requiref(L, "process", &ProcessModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::process
