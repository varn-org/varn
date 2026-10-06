#include "HttpClientLuaFunction.h"

#include "varn/runtime/Runtime.h"

#include <lua.hpp>

namespace varn::http::client
{

// A Lua function a request calls later, held in the registry until the loop releases it, whatever thread drops the last owner.
HttpClientLuaFunction::HttpClientLuaFunction(varn::runtime::Runtime& runtime, int ref)
    : runtime(runtime)
    , ref(ref)
{
}

// The last owner of a request may be a thread of a transport or of a pool, such as the job that moves a downloaded file, so the reference is released by a job of the loop, the one thread that touches the registry.
// A runtime that stopped closes its whole registry, so the reference is left to it then.
HttpClientLuaFunction::~HttpClientLuaFunction()
{
    if (runtime.stopped())
    {
        return;
    }

    varn::runtime::Runtime* owner = &runtime;
    const int released = ref;

    // clang-format off
    runtime.scheduler().mainLoop().post([owner, released]
    {
        if (!owner->stopped())
        {
            luaL_unref(owner->luaState(), LUA_REGISTRYINDEX, released);
        }
    });
    // clang-format on
}

void HttpClientLuaFunction::push(lua_State* L) const
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
}

} // namespace varn::http::client
