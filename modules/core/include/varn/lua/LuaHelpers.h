#pragma once

#include <lua.hpp>
#include <string>
#include <utility>
#include <vector>

namespace varn::lua
{

class LuaHelpers
{
public:
    LuaHelpers() = delete;

    static void pushStringMap(lua_State* L, const std::vector<std::pair<std::string, std::string>>& values);
    static std::string checkString(lua_State* L, int index);
    static std::string optionalString(lua_State* L, int index, const std::string& fallback = {});
    static int traceback(lua_State* L);
    static void pushRuntime(lua_State* L, void* runtime);
    static void* getRuntime(lua_State* L);
};

} // namespace varn::lua
