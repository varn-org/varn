#include "varn/lua/LuaEngine.h"
#include "varn/log/Log.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/lua/NativeModules.h"
#include "varn/runtime/Runtime.h"

namespace varn::lua
{

int LuaEngine::handlePanic(lua_State* L)
{
    const char* message = lua_tostring(L, -1);
    log::Log::error("LuaEngine", message ? message : "An unprotected Lua error occurred.");
    return 0;
}

LuaEngine::LuaEngine(varn::runtime::Runtime& runtime)
    : runtime(runtime)
{
    L = luaL_newstate();
    lua_atpanic(L, &LuaEngine::handlePanic);
    luaL_openlibs(L);

    // use generational collection for short-lived garbage over a long-lived application heap
    lua_gc(L, LUA_GCGEN);

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
    lua_pushcfunction(L, &varn::lua::LuaHelpers::traceback);
    int tracebackIndex = lua_gettop(L);

    if (luaL_loadfilex(L, path.c_str(), "t") != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        log::Log::error("LuaEngine", message ? message : "The script could not be loaded.");
        lua_pop(L, 2);
        return 1;
    }

    if (lua_pcall(L, 0, 0, tracebackIndex) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        log::Log::error("LuaEngine", message ? message : "The script failed to run.");
        lua_pop(L, 2);
        return 1;
    }

    lua_pop(L, 1);
    return 0;
}

bool LuaEngine::runStringWithoutEventLoop(const std::string& source, const std::string& chunkName, std::string* errorMessage, LuaPrePcallHook prePcallHook, void* prePcallUserdata)
{
    lua_pushcfunction(L, &varn::lua::LuaHelpers::traceback);
    const int tracebackIndex = lua_gettop(L);

    if (luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK)
    {
        if (errorMessage != nullptr)
        {
            size_t len = 0;
            const char* message = lua_tolstring(L, -1, &len);
            *errorMessage = message ? std::string(message, len) : std::string("Lua load failed.");
        }

        lua_pop(L, 2);
        return false;
    }

    if (prePcallHook != nullptr)
    {
        prePcallHook(L, prePcallUserdata);
    }

    if (lua_pcall(L, 0, 0, tracebackIndex) != LUA_OK)
    {
        if (errorMessage != nullptr)
        {
            size_t len = 0;
            const char* message = lua_tolstring(L, -1, &len);
            *errorMessage = message ? std::string(message, len) : std::string("Lua runtime failed.");
        }

        lua_pop(L, 2);
        return false;
    }

    lua_pop(L, 1);
    return true;
}

int LuaEngine::runString(const std::string& source, const std::string& chunkName)
{
    lua_pushcfunction(L, &varn::lua::LuaHelpers::traceback);
    int tracebackIndex = lua_gettop(L);

    if (luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        log::Log::error("LuaEngine", message ? message : "The source could not be loaded.");
        lua_pop(L, 2);
        return 1;
    }

    if (lua_pcall(L, 0, 0, tracebackIndex) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        log::Log::error("LuaEngine", message ? message : "The source failed to run.");
        lua_pop(L, 2);
        return 1;
    }

    lua_pop(L, 1);
    return 0;
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

    // index args so the chunk is arg[0], its arguments arg[1..] and the program options arg[-1..]
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
