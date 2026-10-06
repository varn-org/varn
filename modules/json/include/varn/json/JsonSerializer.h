#pragma once

#include "varn/dataview/DataView.h"

#include <lua.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace varn::json
{

class JsonSerializer
{
public:
    JsonSerializer() = delete;

    static constexpr const char* arrayMetatable = "varn.json.array";
    static constexpr const char* objectMetatable = "varn.json.object";

    static std::string serialize(lua_State* L, int index);
    static bool deserialize(lua_State* L, const std::string& text);
    static void encode(lua_State* L, int index, int indent);
    static bool decode(lua_State* L, std::string_view text, int nullIndex, std::string& error);

    static std::string serializeBridge(lua_State* L, int index);
    static bool deserializeBridge(lua_State* L, std::string_view text, std::string& error);
    static std::vector<varn::dataview::DataView> acquireViews(std::string_view text);
};

} // namespace varn::json
