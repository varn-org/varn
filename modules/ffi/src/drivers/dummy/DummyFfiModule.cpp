#include <lua.hpp>

namespace varn::ffi
{

class DummyFfi
{
public:
    static int disabled(lua_State* L)
    {
        return luaL_error(L, "[DummyFfi] The FFI module is not available in this build.");
    }

    static constexpr luaL_Reg lib[] = {
        {"cdef", disabled},
        {"load", disabled},
        {"new", disabled},
        {"cast", disabled},
        {"metatype", disabled},
        {"typeof", disabled},
        {"addressof", disabled},
        {"gc", disabled},
        {"sizeof", disabled},
        {"offsetof", disabled},
        {"istype", disabled},
        {"tonumber", disabled},
        {"string", disabled},
        {"copy", disabled},
        {"fill", disabled},
        {"errno", disabled},
        {nullptr, nullptr},
    };
};

} // namespace varn::ffi

extern "C" int luaopen_ffi_dummy(lua_State* L)
{
    luaL_newlib(L, varn::ffi::DummyFfi::lib);
    lua_pushliteral(L, "0-dummy");
    lua_setfield(L, -2, "VERSION");
    lua_pushnil(L);
    lua_setfield(L, -2, "nullptr");
    lua_newtable(L);
    lua_setfield(L, -2, "C");
    return 1;
}
