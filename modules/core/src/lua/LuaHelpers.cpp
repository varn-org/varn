#include "varn/lua/LuaHelpers.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>

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
        lua_pushliteral(L, "An unknown Lua error occurred.");
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
        luaL_error(L, "The runtime is not available.");
    }

    return runtime;
}

// Reads the indentation of the options table of an encoder, refusing a width that is not a whole number of spaces from 0 to 16.
int LuaHelpers::readIndent(lua_State* L, int optsIndex)
{
    if (lua_isnoneornil(L, optsIndex))
    {
        return 0;
    }

    luaL_checktype(L, optsIndex, LUA_TTABLE);

    constexpr lua_Integer kMaxIndent = 16;
    int indent = 0;
    lua_getfield(L, optsIndex, "indent");
    if (!lua_isnil(L, -1))
    {
        int isInteger = 0;
        const lua_Integer value = lua_type(L, -1) == LUA_TNUMBER ? lua_tointegerx(L, -1, &isInteger) : 0;
        if (isInteger == 0 || value < 0 || value > kMaxIndent)
        {
            luaL_error(L, "The option \"indent\" must be a whole number of spaces from 0 to %I.", kMaxIndent);
        }

        indent = static_cast<int>(value);
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

// Answers the line that `print` writes for its arguments, each one as `tostring` gives it, separated by tabs.
std::string LuaHelpers::printedLine(lua_State* L)
{
    const int count = lua_gettop(L);
    std::string line;
    for (int index = 1; index <= count; ++index)
    {
        if (index > 1)
        {
            line += '\t';
        }

        std::size_t length = 0;
        const char* text = luaL_tolstring(L, index, &length);
        line.append(text, length);
        lua_pop(L, 1);
    }

    return line;
}

// Refuses an option of the table at the index that the call does not take, so a misspelled option never passes in silence.
void LuaHelpers::refuseUnknownOptions(lua_State* L, int index, const char* kind, std::span<const std::string_view> known)
{
    const int table = lua_absindex(L, index);
    lua_pushnil(L);
    while (lua_next(L, table) != 0)
    {
        if (lua_type(L, -2) != LUA_TSTRING)
        {
            luaL_error(L, "An option must be named by a string.");
        }

        const std::string_view name = lua_tostring(L, -2);
        if (std::ranges::find(known, name) == known.end())
        {
            luaL_error(L, "The %s option \"%s\" is unknown.", kind, lua_tostring(L, -2));
        }

        lua_pop(L, 1);
    }
}

} // namespace varn::lua
