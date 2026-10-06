#pragma once

struct lua_State;

namespace varn::lua
{

class NativeModuleRegistry
{
public:
    NativeModuleRegistry() = delete;

    static void installAll(lua_State* L);
};

} // namespace varn::lua
