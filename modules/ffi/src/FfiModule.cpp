#include "varn/ffi/FfiModule.h"

#include <lua.hpp>

#if defined(VARN_FFI_DRIVER_LIBFFI)
#include "varn/async/AsyncModule.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"
#include "varn_ffi_host.h"

#include <new>
#include <string>
#include <thread>
#include <type_traits>
#endif

#if defined(VARN_FFI_DRIVER_DUMMY)
extern "C" int luaopen_ffi_dummy(lua_State* L);
#endif

namespace varn::ffi
{

#if defined(VARN_FFI_DRIVER_LIBFFI)
namespace
{
// The services the runtime lends the binding, which answer on any thread without touching the Lua state.
class FfiHost : public varn_ffi_host
{
public:
    explicit FfiHost(runtime::Runtime& runtime)
        : varn_ffi_host{&FfiHost::onLuaThread, &async::AsyncModule::fail, &FfiHost::postFailure, &async::AsyncModule::capture, &async::AsyncModule::raise}
        , runtime(runtime)
        , luaThread(std::this_thread::get_id())
    {
    }

private:
    runtime::Runtime& runtime;
    std::thread::id luaThread;

    static bool onLuaThread(const varn_ffi_host* host) { return std::this_thread::get_id() == static_cast<const FfiHost*>(host)->luaThread; }

    // Posts the failure of a callback called from another thread to the loop, which hands it to the failure handler on the thread that runs Lua.
    static void postFailure(const varn_ffi_host* host, const char* message)
    {
        runtime::Runtime& target = static_cast<const FfiHost*>(host)->runtime;

        // The caller is native code that cannot take an exception, so a post that cannot even allocate leaves the failure behind.
        try
        {
            // clang-format off
            target.mainLoop().post([&target, failure = std::string(message)]
            {
                if (target.stopped())
                {
                    return;
                }

                lua_State* L = target.luaState();
                lua_createtable(L, 0, 1);
                lua_pushlstring(L, failure.data(), failure.size());
                lua_setfield(L, -2, "error");
                async::AsyncModule::fail(L);
            });
            // clang-format on
        }
        catch (...)
        {
        }
    }
};

static_assert(std::is_trivially_destructible_v<FfiHost>, "The host lives in a userdata that is never finalized.");
} // namespace
#endif

// Installs the module as `ffi`, recording the thread that runs Lua so a callback can tell when another thread calls it.
void FfiModule::install(lua_State* L)
{
#if defined(VARN_FFI_DRIVER_LIBFFI)
    auto* host = new (lua_newuserdatauv(L, sizeof(FfiHost), 0)) FfiHost(*static_cast<runtime::Runtime*>(lua::LuaHelpers::getRuntime(L)));
    lua_setfield(L, LUA_REGISTRYINDEX, "varn.ffi.host");

    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    varn_ffi_open(L, host);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "ffi");
    lua_setglobal(L, "ffi");
    lua_pop(L, 1);
#elif defined(VARN_FFI_DRIVER_DUMMY)
    luaL_requiref(L, "ffi", luaopen_ffi_dummy, 1);
    lua_pop(L, 1);
#else
#error "The \"VARN_FFI_DRIVER\" option must define \"LIBFFI\" or \"DUMMY\"."
#endif
}

} // namespace varn::ffi
