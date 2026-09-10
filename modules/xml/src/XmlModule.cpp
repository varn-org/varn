#include "varn/xml/XmlModule.h"
#include "varn/xml/XmlSerializer.h"

#include <lua.hpp>

#include <algorithm>
#include <exception>
#include <string>

namespace varn::xml
{

int XmlModule::readIndent(lua_State* L, int optsIndex)
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

int XmlModule::luaEncode(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const int indent = readIndent(L, 2);
    try
    {
        const std::string out = XmlSerializer::encodeNode(L, 1, indent);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int XmlModule::luaDecode(lua_State* L)
{
    std::size_t length = 0;
    const char* text = luaL_checklstring(L, 1, &length);
    try
    {
        if (!XmlSerializer::parse(L, std::string(text, length)))
        {
            return luaL_error(L, "[XmlModule] The input is not valid XML.");
        }

        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int XmlModule::luaOpen(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, &XmlModule::luaEncode);
    lua_setfield(L, -2, "encode");
    lua_pushcfunction(L, &XmlModule::luaEncode);
    lua_setfield(L, -2, "stringify");

    lua_pushcfunction(L, &XmlModule::luaDecode);
    lua_setfield(L, -2, "decode");
    lua_pushcfunction(L, &XmlModule::luaDecode);
    lua_setfield(L, -2, "parse");

    return 1;
}

void XmlModule::install(lua_State* L)
{
    luaL_requiref(L, "xml", &XmlModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::xml
