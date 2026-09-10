#pragma once

struct lua_State;

namespace varn::runtime
{
class Runtime;
}

namespace varn::http
{

class HttpClientModule
{
public:
    HttpClientModule() = delete;

    static void registerClient(lua_State* L);

private:
    static int luaClientRequest(lua_State* L);
    static int luaClientStream(lua_State* L);
    static void installPrelude(lua_State* L);
    static varn::runtime::Runtime& luaRuntime(lua_State* L);
};

} // namespace varn::http
