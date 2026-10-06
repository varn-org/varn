#include "varn/json/JsonSerializer.h"

#include "NlohmannJsonDecoder.h"
#include "NlohmannJsonEncoder.h"

#include <string>

namespace varn::json
{

// Every Lua value is valid JSON, so a string or a number crosses the bridge as itself rather than as an empty object.
std::string JsonSerializer::serialize(lua_State* L, int index)
{
    encode(L, index, 0);
    std::size_t length = 0;
    const char* text = lua_tolstring(L, -1, &length);
    std::string out(text, length);
    lua_pop(L, 1);
    return out;
}

bool JsonSerializer::deserialize(lua_State* L, const std::string& text)
{
    std::string error;
    return decode(L, text, 0, error);
}

void JsonSerializer::encode(lua_State* L, int index, int indent)
{
    JsonEncoder::encode(L, index, indent);
}

bool JsonSerializer::decode(lua_State* L, std::string_view text, int nullIndex, std::string& error)
{
    return JsonDecoder::decode(L, text, nullIndex, error);
}

} // namespace varn::json
