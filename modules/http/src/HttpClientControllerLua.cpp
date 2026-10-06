#include "HttpClientControllerLua.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace varn::http::client
{

namespace
{
constexpr const char* kControllerMeta = "varn.http.controller";
} // namespace

// Makes a controller, which closes as a to-be-closed variable by cancelling its requests, so a task that ends or is cancelled takes them with it.
int HttpClientControllerLua::create(lua_State* L)
{
    void* memory = lua_newuserdatauv(L, sizeof(Controller), 0);
    new (memory) Controller();

    if (luaL_newmetatable(L, kControllerMeta))
    {
        lua_pushcfunction(L, &HttpClientControllerLua::index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, &HttpClientControllerLua::cancel);
        lua_setfield(L, -2, "__close");
        lua_pushcfunction(L, &HttpClientControllerLua::gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

// Reads the option `controller` of the options table at `options`, before the request starts, so a wrong value never leaves a request running.
HttpClientControllerLua::Controller* HttpClientControllerLua::option(lua_State* L, int options)
{
    lua_getfield(L, options, "controller");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return nullptr;
    }

    auto* controller = static_cast<Controller*>(luaL_testudata(L, -1, kControllerMeta));
    lua_pop(L, 1);
    if (controller == nullptr)
    {
        luaL_error(L, "The option \"controller\" must be a controller of \"http.client.controller\".");
    }

    return controller;
}

// Lets the controller cancel the request, at once when it was already cancelled.
void HttpClientControllerLua::attach(Controller* controller, const Handle& handle)
{
    if (controller == nullptr)
    {
        return;
    }

    if (controller->cancelled)
    {
        handle.cancel();
        return;
    }

    // clang-format off
    controller->handles.erase(std::remove_if(controller->handles.begin(), controller->handles.end(), [](const Handle& known) { return !known.active(); }), controller->handles.end());
    // clang-format on
    controller->handles.push_back(handle);
}

HttpClientControllerLua::Controller& HttpClientControllerLua::check(lua_State* L)
{
    return *static_cast<Controller*>(luaL_checkudata(L, 1, kControllerMeta));
}

void HttpClientControllerLua::cancelAll(Controller& controller)
{
    controller.cancelled = true;
    const std::vector<Handle> running = std::exchange(controller.handles, {});
    for (const Handle& handle : running)
    {
        handle.cancel();
    }
}

int HttpClientControllerLua::cancel(lua_State* L)
{
    cancelAll(check(L));
    return 0;
}

int HttpClientControllerLua::index(lua_State* L)
{
    Controller& controller = check(L);
    const char* key = luaL_checkstring(L, 2);

    if (std::strcmp(key, "cancel") == 0)
    {
        lua_pushcfunction(L, &HttpClientControllerLua::cancel);
        return 1;
    }

    if (std::strcmp(key, "cancelled") == 0)
    {
        lua_pushboolean(L, controller.cancelled ? 1 : 0);
        return 1;
    }

    return 0;
}

int HttpClientControllerLua::gc(lua_State* L)
{
    check(L).~Controller();
    return 0;
}

} // namespace varn::http::client
