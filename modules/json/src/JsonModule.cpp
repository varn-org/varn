#include "varn/json/JsonModule.h"
#include "varn/json/JsonSerializer.h"

#include <lua.hpp>

#include <algorithm>
#include <exception>
#include <string>

namespace varn::json
{

int JsonModule::readIndent(lua_State* L, int optsIndex)
{
    if (!lua_istable(L, optsIndex))
    {
        return 0;
    }

    int indent = 0;
    lua_getfield(L, optsIndex, "indent");
    if (lua_isinteger(L, -1))
    {
        const lua_Integer value = lua_tointeger(L, -1);
        indent = static_cast<int>(std::clamp<lua_Integer>(value, 0, 16));
    }

    lua_pop(L, 1);

    lua_getfield(L, optsIndex, "pretty");
    if (lua_toboolean(L, -1) && indent <= 0)
    {
        indent = 2;
    }

    lua_pop(L, 1);

    return indent;
}

int JsonModule::luaEncode(lua_State* L)
{
    const int indent = readIndent(L, 2);
    try
    {
        const std::string out = JsonSerializer::encode(L, 1, indent);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int JsonModule::luaDecode(lua_State* L)
{
    std::size_t length = 0;
    const char* text = luaL_checklstring(L, 1, &length);
    try
    {
        if (!JsonSerializer::deserialize(L, std::string(text, length)))
        {
            return luaL_error(L, "[JsonModule] The input is not valid JSON.");
        }

        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int JsonModule::luaOpen(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, &JsonModule::luaEncode);
    lua_setfield(L, -2, "encode");
    lua_pushcfunction(L, &JsonModule::luaEncode);
    lua_setfield(L, -2, "stringify");

    lua_pushcfunction(L, &JsonModule::luaDecode);
    lua_setfield(L, -2, "decode");
    lua_pushcfunction(L, &JsonModule::luaDecode);
    lua_setfield(L, -2, "parse");

    return 1;
}

void JsonModule::install(lua_State* L)
{
    luaL_requiref(L, "json", &JsonModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::json
