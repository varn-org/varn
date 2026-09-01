#pragma once

#include <lua.hpp>
#include <nlohmann/json.hpp>

#include <string>

namespace varn::json
{

class JsonConvert
{
public:
    JsonConvert() = delete;

    static std::string dumpString(const std::string& value);
    static bool isSequence(lua_State* L, int index, lua_Integer& length);
    static void pushJson(lua_State* L, const nlohmann::json& value, int depth);
};

} // namespace varn::json
