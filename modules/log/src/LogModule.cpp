#include "varn/log/LogModule.h"

#include "varn/log/Log.h"

#include <lua.hpp>
#include <string>
#include <string_view>

namespace varn::log
{

void LogModule::emitAt(lua_State* L, Level level)
{
    const int n = lua_gettop(L);
    if (n == 0)
    {
        return;
    }

    // treat a trailing table as structured key=value fields
    int messageCount = n;
    if (n >= 2 && lua_type(L, n) == LUA_TTABLE)
    {
        messageCount = n - 1;
    }

    std::string combined;
    combined.reserve(64);
    for (int i = 1; i <= messageCount; ++i)
    {
        if (i > 1)
        {
            combined += '\t';
        }

        appendValue(L, i, combined, 0, false);
    }

    if (messageCount < n)
    {
        appendFields(L, n, combined);
    }

    Log::emit(level, combined);
}

void LogModule::appendFields(lua_State* L, int index, std::string& out)
{
    index = lua_absindex(L, index);

    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        out += ' ';
        appendKey(L, -2, out);
        out += '=';
        appendValue(L, -1, out, 1, false);

        lua_pop(L, 1);
    }
}

void LogModule::appendValue(lua_State* L, int index, std::string& out, int depth, bool quoteStrings)
{
    index = lua_absindex(L, index);

    if (lua_type(L, index) == LUA_TTABLE)
    {
        appendTable(L, index, out, depth);
        return;
    }

    if (quoteStrings && lua_type(L, index) == LUA_TSTRING)
    {
        size_t len = 0;
        const char* text = lua_tolstring(L, index, &len);
        out += '"';
        out.append(text, len);
        out += '"';
        return;
    }

    // pop the text pushed by luaL_tolstring after appending it
    size_t len = 0;
    const char* text = luaL_tolstring(L, index, &len);
    out.append(text, len);
    lua_pop(L, 1);
}

void LogModule::appendKey(lua_State* L, int index, std::string& out)
{
    index = lua_absindex(L, index);

    if (lua_type(L, index) == LUA_TSTRING)
    {
        size_t len = 0;
        const char* key = lua_tolstring(L, index, &len);
        out.append(key, len);
        return;
    }

    // coerce a copy so lua_next still sees the original key
    out += '[';
    lua_pushvalue(L, index);
    size_t len = 0;
    const char* key = luaL_tolstring(L, -1, &len);
    out.append(key, len);
    lua_pop(L, 2);
    out += ']';
}

void LogModule::appendTable(lua_State* L, int index, std::string& out, int depth)
{
    constexpr int maxDepth = 4;
    constexpr int maxEntries = 32;

    if (depth >= maxDepth)
    {
        out += "{...}";
        return;
    }

    index = lua_absindex(L, index);

    out += '{';
    int count = 0;

    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        if (count >= maxEntries)
        {
            out += " ...";
            lua_pop(L, 2);
            break;
        }

        out += (count == 0) ? " " : ", ";
        ++count;

        appendKey(L, -2, out);
        out += " = ";
        appendValue(L, -1, out, depth + 1, true);

        lua_pop(L, 1);
    }

    out += (count == 0) ? "}" : " }";
}

int LogModule::luaDebug(lua_State* L)
{
    emitAt(L, Level::Debug);
    return 0;
}

int LogModule::luaInfo(lua_State* L)
{
    emitAt(L, Level::Info);
    return 0;
}

int LogModule::luaWarn(lua_State* L)
{
    emitAt(L, Level::Warn);
    return 0;
}

int LogModule::luaError(lua_State* L)
{
    emitAt(L, Level::Error);
    return 0;
}

int LogModule::luaSetLevel(lua_State* L)
{
    size_t len = 0;
    const char* name = luaL_checklstring(L, 1, &len);
    const std::string_view level(name, len);

    if (level == "debug")
    {
        Log::setLevel(Level::Debug);
    }
    else if (level == "info")
    {
        Log::setLevel(Level::Info);
    }
    else if (level == "warn")
    {
        Log::setLevel(Level::Warn);
    }
    else if (level == "error")
    {
        Log::setLevel(Level::Error);
    }
    else
    {
        return luaL_error(L, "log.setLevel expects one of: debug, info, warn, error");
    }

    return 0;
}

int LogModule::luaToFile(lua_State* L)
{
    size_t len = 0;
    const char* path = luaL_checklstring(L, 1, &len);
    const bool rotating = lua_toboolean(L, 2) != 0;

    Log::addFileSink(std::string_view(path, len), rotating);
    return 0;
}

int LogModule::luaOpen(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, &LogModule::luaDebug);
    lua_setfield(L, -2, "debug");

    lua_pushcfunction(L, &LogModule::luaInfo);
    lua_setfield(L, -2, "info");

    lua_pushcfunction(L, &LogModule::luaWarn);
    lua_setfield(L, -2, "warn");

    lua_pushcfunction(L, &LogModule::luaError);
    lua_setfield(L, -2, "error");

    lua_pushcfunction(L, &LogModule::luaSetLevel);
    lua_setfield(L, -2, "setLevel");

    lua_pushcfunction(L, &LogModule::luaToFile);
    lua_setfield(L, -2, "toFile");

    return 1;
}

void LogModule::install(lua_State* L)
{
    luaL_requiref(L, "log", &LogModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::log
