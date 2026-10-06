#pragma once

struct lua_State;

namespace varn::platform
{

class PlatformModule
{
public:
    PlatformModule() = delete;
    static void install(lua_State* L);

private:
    static int luaOs(lua_State* L);
    static int luaArch(lua_State* L);
    static int luaLibPrefix(lua_State* L);
    static int luaShlibSuffix(lua_State* L);
    static int luaLibraryFilename(lua_State* L);
    static int luaHostVersion(lua_State* L);
    static void pushVersionTable(lua_State* L);
    static int luaCpuCount(lua_State* L);
    static int luaPointerSize(lua_State* L);
    static int luaEndianness(lua_State* L);
    static int luaLibraryPathByName(lua_State* L);
    static int luaOpen(lua_State* L);
};

} // namespace varn::platform
