#pragma once

struct lua_State;

namespace varn::fs
{

class FsModule
{
public:
    FsModule() = delete;

    static void install(lua_State* L);

private:
    static int luaReadFile(lua_State* L);
    static int luaWriteFile(lua_State* L);
    static int luaExists(lua_State* L);
    static int luaMkdir(lua_State* L);
    static int luaRemoveRecursive(lua_State* L);
    static int luaOpen(lua_State* L);
};

} // namespace varn::fs
