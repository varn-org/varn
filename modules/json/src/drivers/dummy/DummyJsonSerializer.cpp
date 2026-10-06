#include "varn/json/JsonSerializer.h"

#include <stdexcept>
#include <string>

namespace varn::json
{

std::string JsonSerializer::serialize(lua_State* /*L*/, int /*index*/)
{
    throw std::runtime_error("[JsonSerializer] The JSON module is not available in this build.");
}

bool JsonSerializer::deserialize(lua_State* /*L*/, const std::string& /*text*/)
{
    throw std::runtime_error("[JsonSerializer] The JSON module is not available in this build.");
}

void JsonSerializer::encode(lua_State* L, int /*index*/, int /*indent*/)
{
    luaL_error(L, "[JsonSerializer] The JSON module is not available in this build.");
}

bool JsonSerializer::decode(lua_State* /*L*/, std::string_view /*text*/, int /*nullIndex*/, std::string& error)
{
    error = "The JSON module is not available in this build.";
    return false;
}

} // namespace varn::json
