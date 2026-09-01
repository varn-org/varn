#pragma once

struct lua_State;

namespace varn::xml
{

class XmlModule
{
public:
    XmlModule() = delete;

    static void install(struct lua_State* L);

private:
    static int luaEncode(struct lua_State* L);
    static int luaDecode(struct lua_State* L);
    static int luaOpen(struct lua_State* L);
    static int readIndent(struct lua_State* L, int optsIndex);
};

} // namespace varn::xml
