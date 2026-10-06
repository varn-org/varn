#include "varn/http/HttpClientModule.h"
#include "varn/http/HttpServerModule.h"

#include <lua.hpp>

#if VARN_HTTP_SERVER_DRIVER_POCO
#error "The file \"HttpServerModuleStub.cpp\" is only for non-POCO HTTP server drivers. Use \"HttpServerModule.cpp\" instead."
#endif

namespace varn::http
{

class HttpServerLuaStub
{
public:
    static int luaOpen(lua_State* L);

private:
    static constexpr const char* kServerFunctions[] = {"createServer", "createApp", "cors", "securityHeaders", "apiKey", "rateLimit", "jwtAuth", "csrf", "requireAuth", "requireRole"};
    static constexpr const char* kJwtFunctions[] = {"sign", "verify"};

    static int luaUnavailable(lua_State* L);
};

int HttpServerLuaStub::luaUnavailable(lua_State* L)
{
    return luaL_error(L, "The HTTP server is not available in this build.");
}

// Every function of the server exists and fails with the same sentence, so a script learns what this build lacks rather than calling a missing field.
int HttpServerLuaStub::luaOpen(lua_State* L)
{
    lua_newtable(L);
    for (const char* name : kServerFunctions)
    {
        lua_pushcfunction(L, &HttpServerLuaStub::luaUnavailable);
        lua_setfield(L, -2, name);
    }

    lua_newtable(L);
    for (const char* name : kJwtFunctions)
    {
        lua_pushcfunction(L, &HttpServerLuaStub::luaUnavailable);
        lua_setfield(L, -2, name);
    }

    lua_setfield(L, -2, "jwt");
    HttpClientModule::registerClient(L);
    return 1;
}

void HttpServerModule::install(lua_State* L)
{
    luaL_requiref(L, "http", &HttpServerLuaStub::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::http
