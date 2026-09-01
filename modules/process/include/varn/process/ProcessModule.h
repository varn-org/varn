#pragma once

struct lua_State;

namespace varn::runtime
{
class Runtime;
}

namespace varn::process
{

class ProcessModule
{
public:
    ProcessModule() = delete;

    static void install(lua_State* L);

private:
    static varn::runtime::Runtime& luaRuntime(lua_State* L);

    static int luaExec(lua_State* L);
    static int luaGetenv(lua_State* L);
    static int luaCwd(lua_State* L);
    static int luaOpen(lua_State* L);
};

} // namespace varn::process
