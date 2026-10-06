#pragma once

#include "varn/http/HttpClientHandle.h"

#include <lua.hpp>

#include <vector>

namespace varn::http::client
{

/// The `Controller` of `http.client.controller()`, which cancels every request that names it in its option `controller`.
class HttpClientControllerLua
{
public:
    HttpClientControllerLua() = delete;

    struct Controller
    {
        std::vector<Handle> handles;
        bool cancelled = false;
    };

    static int create(lua_State* L);
    static Controller* option(lua_State* L, int options);
    static void attach(Controller* controller, const Handle& handle);

private:
    static Controller& check(lua_State* L);
    static void cancelAll(Controller& controller);
    static int cancel(lua_State* L);
    static int index(lua_State* L);
    static int gc(lua_State* L);
};

} // namespace varn::http::client
