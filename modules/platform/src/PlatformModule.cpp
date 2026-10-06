#include "varn/platform/PlatformModule.h"

#include "VarnVersion.h"
#include "varn/platform/PlatformInfo.h"

#include <lua.hpp>

#include <exception>
#include <string>

namespace varn::platform
{

int PlatformModule::luaOs(lua_State* L)
{
    const std::string v = PlatformInfo::osId();
    lua_pushlstring(L, v.data(), v.size());
    return 1;
}

int PlatformModule::luaArch(lua_State* L)
{
    const std::string v = PlatformInfo::archId();
    lua_pushlstring(L, v.data(), v.size());
    return 1;
}

int PlatformModule::luaLibPrefix(lua_State* L)
{
    const std::string v = PlatformInfo::libPrefix();
    lua_pushlstring(L, v.data(), v.size());
    return 1;
}

int PlatformModule::luaShlibSuffix(lua_State* L)
{
    const std::string v = PlatformInfo::shlibSuffix();
    lua_pushlstring(L, v.data(), v.size());
    return 1;
}

int PlatformModule::luaLibraryFilename(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    try
    {
        const std::string path = PlatformInfo::libraryFilenameForName(name);
        lua_pushlstring(L, path.data(), path.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

int PlatformModule::luaHostVersion(lua_State* L)
{
    lua_pushstring(L, VARN_VERSION_STRING);
    return 1;
}

int PlatformModule::luaCpuCount(lua_State* L)
{
    lua_pushinteger(L, static_cast<lua_Integer>(PlatformInfo::cpuCount()));
    return 1;
}

int PlatformModule::luaPointerSize(lua_State* L)
{
    lua_pushinteger(L, static_cast<lua_Integer>(PlatformInfo::pointerSize()));
    return 1;
}

int PlatformModule::luaEndianness(lua_State* L)
{
    const std::string v = PlatformInfo::endianness();
    lua_pushlstring(L, v.data(), v.size());
    return 1;
}

int PlatformModule::luaLibraryPathByName(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* subdirRaw = luaL_optstring(L, 2, ".");
    try
    {
        std::string out = subdirRaw;
        const std::string file = PlatformInfo::libraryFilenameForName(name);
        if (!out.empty() && out.back() != '/' && out.back() != '\\')
        {
            out += '/';
        }

        out += file;
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, "%s", ex.what());
    }
}

void PlatformModule::pushVersionTable(lua_State* L)
{
    lua_createtable(L, 0, 4);

    lua_pushinteger(L, VARN_VERSION_MAJOR);
    lua_setfield(L, -2, "major");
    lua_pushinteger(L, VARN_VERSION_MINOR);
    lua_setfield(L, -2, "minor");
    lua_pushinteger(L, VARN_VERSION_PATCH);
    lua_setfield(L, -2, "patch");
    lua_pushstring(L, VARN_VERSION_STRING);
    lua_setfield(L, -2, "string");
}

int PlatformModule::luaOpen(lua_State* L)
{
    lua_newtable(L);

    PlatformModule::pushVersionTable(L);
    lua_setfield(L, -2, "version");

    lua_pushcfunction(L, &PlatformModule::luaOs);
    lua_setfield(L, -2, "os");

    lua_pushcfunction(L, &PlatformModule::luaArch);
    lua_setfield(L, -2, "arch");

    lua_pushcfunction(L, &PlatformModule::luaHostVersion);
    lua_setfield(L, -2, "hostVersion");

    lua_pushcfunction(L, &PlatformModule::luaCpuCount);
    lua_setfield(L, -2, "cpuCount");

    lua_pushcfunction(L, &PlatformModule::luaPointerSize);
    lua_setfield(L, -2, "pointerSize");

    lua_pushcfunction(L, &PlatformModule::luaEndianness);
    lua_setfield(L, -2, "endianness");

    lua_pushcfunction(L, &PlatformModule::luaLibPrefix);
    lua_setfield(L, -2, "libPrefix");

    lua_pushcfunction(L, &PlatformModule::luaShlibSuffix);
    lua_setfield(L, -2, "shlibSuffix");

    lua_pushcfunction(L, &PlatformModule::luaLibraryFilename);
    lua_setfield(L, -2, "libraryFilename");

    lua_pushcfunction(L, &PlatformModule::luaLibraryPathByName);
    lua_setfield(L, -2, "getLibraryPathByName");

    return 1;
}

void PlatformModule::install(lua_State* L)
{
    luaL_requiref(L, "platform", &PlatformModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::platform
