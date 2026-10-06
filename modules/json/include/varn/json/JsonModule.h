#pragma once

struct lua_State;

namespace varn::json
{

class JsonModule
{
public:
    JsonModule() = delete;

    static void install(struct lua_State* L);

private:
    static int luaEncode(struct lua_State* L);
    static int luaDecode(struct lua_State* L);
    static int luaArray(struct lua_State* L);
    static int luaObject(struct lua_State* L);
    static int luaOpen(struct lua_State* L);
    static int readIndent(struct lua_State* L, int optsIndex);
    static int mark(struct lua_State* L, const char* metatable);
};

} // namespace varn::json
