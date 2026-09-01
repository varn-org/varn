#include "varn/lua/LuaHelpers.h"

#include <stdexcept>

namespace varn::lua
{

void LuaHelpers::pushStringMap(lua_State* L, const std::vector<std::pair<std::string, std::string>>& values)
{
    lua_newtable(L);

    for (const auto& [key, value] : values)
    {
        lua_pushlstring(L, value.data(), value.size());
        lua_setfield(L, -2, key.c_str());
    }
}

std::string LuaHelpers::checkString(lua_State* L, int index)
{
    size_t len = 0;
    const char* value = luaL_checklstring(L, index, &len);
    return std::string(value, len);
}

std::string LuaHelpers::optionalString(lua_State* L, int index, const std::string& fallback)
{
    if (lua_isnoneornil(L, index))
    {
        return fallback;
    }

    return checkString(L, index);
}

int LuaHelpers::traceback(lua_State* L)
{
    const char* message = lua_tostring(L, 1);

    if (message)
    {
        luaL_traceback(L, L, message, 1);
    }
    else
    {
        lua_pushliteral(L, "unknown lua error");
    }

    return 1;
}

void LuaHelpers::pushRuntime(lua_State* L, void* runtime)
{
    lua_pushlightuserdata(L, runtime);
    lua_setfield(L, LUA_REGISTRYINDEX, "varn.runtime");
}

void* LuaHelpers::getRuntime(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "varn.runtime");
    void* runtime = lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!runtime)
    {
        luaL_error(L, "[LuaHelpers] The runtime is not available.");
    }

    return runtime;
}

} // namespace varn::lua
