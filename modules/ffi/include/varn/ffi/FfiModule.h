#pragma once

struct lua_State;

namespace varn::ffi
{

class FfiModule
{
public:
    FfiModule() = delete;

    static void install(lua_State* L);
};

} // namespace varn::ffi
