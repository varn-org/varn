#include "varn/ffi/FfiModule.h"

#include <lua.hpp>

#if defined(VARN_FFI_DRIVER_LIBFFI)
extern "C"
{
    int luaopen_ffi(lua_State* L);
}
#endif

#if defined(VARN_FFI_DRIVER_DUMMY)
extern "C" int luaopen_ffi_dummy(lua_State* L);
#endif

namespace varn::ffi
{

void FfiModule::install(lua_State* L)
{
#if defined(VARN_FFI_DRIVER_LIBFFI)
    luaL_requiref(L, "ffi", luaopen_ffi, 1);
#elif defined(VARN_FFI_DRIVER_DUMMY)
    luaL_requiref(L, "ffi", luaopen_ffi_dummy, 1);
#else
#error "VARN_FFI_DRIVER must define LIBFFI or DUMMY"
#endif
    lua_pop(L, 1);
}

} // namespace varn::ffi
