#include "varn/process/ProcessModule.h"
#include "varn/async/AsyncTask.h"
#include "varn/async/Promise.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/process/ProcessRunner.h"
#include "varn/runtime/Runtime.h"

#include <optional>
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
    static void pushEnvTable(lua_State* L)
    {
        const std::vector<std::pair<std::string, std::string>> entries = ProcessRunner::environment();

        lua_createtable(L, 0, static_cast<int>(entries.size()));
        for (const auto& entry : entries)
        {
            lua_pushlstring(L, entry.second.data(), entry.second.size());
            lua_setfield(L, -2, entry.first.c_str());
        }
    }

    static void pushArgvTable(lua_State* L, Runtime& rt)
    {
        // builds a 1-based array of the script arguments only
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
        return luaL_error(L, "[ProcessModule] A command must not contain a null byte.");
    }

    long long timeoutMs = 0;
    if (lua_istable(L, 2))
    {
        lua_getfield(L, 2, "timeoutMs");
        if (lua_isnumber(L, -1))
        {
            timeoutMs = static_cast<long long>(lua_tointeger(L, -1));
            if (timeoutMs < 0)
            {
                lua_pop(L, 1);
                return luaL_error(L, "[ProcessModule] A timeout must not be negative.");
            }
        }

        lua_pop(L, 1);
    }

    auto& rt = luaRuntime(L);

    // clang-format off
    return varn::async::AsyncTask::runOnPool(L, rt, rt.ioPool(), "ProcessModule", [command = std::move(command), timeoutMs](Promise& promise)
    {
        const ProcessResult result = ProcessRunner::exec(command, timeoutMs);
        if (result.timedOut)
        {
            promise.reject("[ProcessModule] The command exceeded its timeout of " + std::to_string(timeoutMs) + " ms and was killed.");
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

    const std::optional<std::string> value = ProcessRunner::getenv(name);
    if (value)
    {
        lua_pushlstring(L, value->data(), value->size());
        return 1;
    }

    // falls back to the optional default argument
    lua_settop(L, 2);
    return 1;
}

int ProcessModule::luaCwd(lua_State* L)
{
    const std::string dir = ProcessRunner::cwd();
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

    ProcessModuleHelpers::pushEnvTable(L);
    lua_setfield(L, -2, "env");

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
