#include "varn/http/HttpClientModule.h"
#include "varn/http/HttpServerModule.h"

#include <lua.hpp>

#if VARN_HTTP_SERVER_DRIVER_POCO
#error "HttpServerModuleStub.cpp is only for non-POCO HTTP server drivers; use HttpServerModule.cpp instead."
#endif

namespace varn::http
{

class HttpServerLuaStub
{
public:
    static int luaOpen(lua_State* L);

private:
    static int luaStubCreateServer(lua_State* L);
};

int HttpServerLuaStub::luaStubCreateServer(lua_State* L)
{
    return luaL_error(L, "[HttpServerLuaStub] The HTTP server is not available in this build.");
}

int HttpServerLuaStub::luaOpen(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, &HttpServerLuaStub::luaStubCreateServer);
    lua_setfield(L, -2, "createServer");
    HttpClientModule::registerClient(L);
    return 1;
}

void HttpServerModule::install(lua_State* L)
{
    luaL_requiref(L, "http", &HttpServerLuaStub::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::http
