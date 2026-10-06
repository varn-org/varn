#include "varn/lua/LuaEngine.h"
#include "varn/log/Log.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/lua/NativeModules.h"
#include "varn/runtime/Runtime.h"

#include <cstddef>
#include <new>
#include <string>

namespace varn::lua
{

// Sends what a script prints to the platform console, which is the tool a developer already watches on every target.
int LuaEngine::consolePrint(lua_State* L)
{
    varn::log::Log::output(varn::lua::LuaHelpers::printedLine(L));
    return 0;
}

int LuaEngine::handlePanic(lua_State* L)
{
    const char* message = lua_tostring(L, -1);
    log::Log::error(message ? message : "An unprotected Lua error occurred.");
    return 0;
}

LuaEngine::LuaEngine(varn::runtime::Runtime& runtime)
    : runtime(runtime)
{
    L = luaL_newstate();
    if (L == nullptr)
    {
        throw std::bad_alloc();
    }

    lua_atpanic(L, &LuaEngine::handlePanic);
    luaL_openlibs(L);

    // Use generational collection for short-lived garbage over a long-lived application heap.
    lua_gc(L, LUA_GCGEN);

    lua_pushcfunction(L, &LuaEngine::consolePrint);
    lua_setglobal(L, "print");

    varn::lua::LuaHelpers::pushRuntime(L, &runtime);
    configureArgTable();
    installNativeModules();
}

LuaEngine::~LuaEngine()
{
    if (L)
    {
        lua_close(L);
    }
}

int LuaEngine::runFile(const std::string& path)
{
    std::string error;
    if (!runLoaded(luaL_loadfilex(L, path.c_str(), "t"), error))
    {
        log::Log::error(error);
        return 1;
    }

    return 0;
}

int LuaEngine::runString(const std::string& source, const std::string& chunkName)
{
    std::string error;
    if (!runLoaded(luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t"), error))
    {
        log::Log::error(error);
        return 1;
    }

    return 0;
}

bool LuaEngine::runStringWithoutEventLoop(const std::string& source, const std::string& chunkName, std::string& errorMessage)
{
    return runLoaded(luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t"), errorMessage);
}

// Runs the chunk a load left on the stack under a handler that keeps its traceback, answering whether it ran and leaving the reason in `error` when the load or the run failed.
bool LuaEngine::runLoaded(int loadStatus, std::string& error)
{
    if (loadStatus != LUA_OK)
    {
        error = messageAt(-1, "The chunk could not be loaded.");
        lua_pop(L, 1);
        return false;
    }

    lua_pushcfunction(L, &varn::lua::LuaHelpers::traceback);
    lua_insert(L, -2);
    const int tracebackIndex = lua_gettop(L) - 1;
    if (lua_pcall(L, 0, 0, tracebackIndex) != LUA_OK)
    {
        error = messageAt(-1, "The chunk failed to run.");
        lua_pop(L, 2);
        return false;
    }

    lua_pop(L, 1);
    return true;
}

std::string LuaEngine::messageAt(int index, const char* fallback)
{
    std::size_t length = 0;
    const char* message = lua_tolstring(L, index, &length);
    return message != nullptr ? std::string(message, length) : std::string(fallback);
}

lua_State* LuaEngine::state()
{
    return L;
}

void LuaEngine::installNativeModules()
{
    varn::lua::NativeModuleRegistry::installAll(L);
}

void LuaEngine::configureArgTable()
{
    lua_newtable(L);

    // Index the arguments so the chunk is `arg[0]`, its arguments `arg[1..]` and the program options `arg[-1..]`.
    const auto& args = runtime.args();
    const int base = static_cast<int>(runtime.scriptArgIndex());
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        lua_pushlstring(L, args[i].data(), args[i].size());
        lua_rawseti(L, -2, static_cast<int>(i) - base);
    }

    lua_setglobal(L, "arg");
}

} // namespace varn::lua
