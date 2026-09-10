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

std::string JsonSerializer::encode(lua_State* /*L*/, int /*index*/, int /*indent*/)
{
    throw std::runtime_error("[JsonSerializer] The JSON module is not available in this build.");
}

} // namespace varn::json
