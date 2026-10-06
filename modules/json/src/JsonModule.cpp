#include "varn/json/JsonModule.h"
#include "varn/json/JsonSerializer.h"
#include "varn/lua/LuaHelpers.h"

#include <lua.hpp>

#include <string>
#include <string_view>

namespace varn::json
{

int JsonModule::luaEncode(lua_State* L)
{
    luaL_checkany(L, 1);
    const int indent = varn::lua::LuaHelpers::readIndent(L, 2);
    JsonSerializer::encode(L, 1, indent);
    return 1;
}

int JsonModule::luaDecode(lua_State* L)
{
    std::size_t length = 0;
    const char* text = luaL_checklstring(L, 1, &length);

    int nullIndex = 0;
    if (!lua_isnoneornil(L, 2))
    {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_getfield(L, 2, "nullValue");
        nullIndex = lua_gettop(L);
    }

    std::string error;
    if (JsonSerializer::decode(L, std::string_view(text, length), nullIndex, error))
    {
        return 1;
    }

    return luaL_error(L, "%s", error.c_str());
}

// Marks the table, or a new one, so it encodes as the JSON container its metatable names whatever keys it holds.
int JsonModule::mark(lua_State* L, const char* metatable)
{
    if (lua_isnoneornil(L, 1))
    {
        lua_settop(L, 0);
        lua_newtable(L);
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);

    if (lua_getmetatable(L, 1) != 0)
    {
        luaL_getmetatable(L, JsonSerializer::kArrayMetatable);
        luaL_getmetatable(L, JsonSerializer::kObjectMetatable);
        if (!lua_rawequal(L, -3, -2) && !lua_rawequal(L, -3, -1))
        {
            return luaL_error(L, "The table already has a metatable, so it cannot be marked as a JSON array or object.");
        }

        lua_settop(L, 1);
    }

    luaL_newmetatable(L, metatable);
    lua_setmetatable(L, 1);
    return 1;
}

int JsonModule::luaArray(lua_State* L)
{
    return mark(L, JsonSerializer::kArrayMetatable);
}

int JsonModule::luaObject(lua_State* L)
{
    return mark(L, JsonSerializer::kObjectMetatable);
}

int JsonModule::luaOpen(lua_State* L)
{
    luaL_newmetatable(L, JsonSerializer::kArrayMetatable);
    luaL_newmetatable(L, JsonSerializer::kObjectMetatable);
    lua_pop(L, 2);

    lua_newtable(L);

    lua_pushcfunction(L, &JsonModule::luaEncode);
    lua_setfield(L, -2, "encode");

    lua_pushcfunction(L, &JsonModule::luaDecode);
    lua_setfield(L, -2, "decode");

    lua_pushcfunction(L, &JsonModule::luaArray);
    lua_setfield(L, -2, "array");
    lua_pushcfunction(L, &JsonModule::luaObject);
    lua_setfield(L, -2, "object");

    // The null pointer stands for JSON null wherever nil cannot, such as inside an array or as a value an object keeps.
    lua_pushlightuserdata(L, nullptr);
    lua_setfield(L, -2, "null");

    return 1;
}

void JsonModule::install(lua_State* L)
{
    luaL_requiref(L, "json", &JsonModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::json
