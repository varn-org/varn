#include "varn/json/JsonSerializer.h"

#include "NlohmannJsonDecoder.h"
#include "NlohmannJsonEncoder.h"
#include "varn/dataview/DataViewJson.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
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
    JsonEncoder::encode(L, index, indent, false);
}

bool JsonSerializer::decode(lua_State* L, std::string_view text, int nullIndex, std::string& error)
{
    return JsonDecoder::decode(L, text, nullIndex, false, error);
}

// Only the bridge of the host writes the token of a data view, so the address of shared memory never reaches the JSON a script sends anywhere else.
std::string JsonSerializer::serializeBridge(lua_State* L, int index)
{
    JsonEncoder::encode(L, index, 0, true);
    std::size_t length = 0;
    const char* text = lua_tolstring(L, -1, &length);
    std::string out(text, length);
    lua_pop(L, 1);
    return out;
}

// Only the bridge of the host turns a token into the view it names, so JSON from the network never reaches memory of the process.
bool JsonSerializer::deserializeBridge(lua_State* L, std::string_view text, std::string& error)
{
    return JsonDecoder::decode(L, text, 0, true, error);
}

// Takes a reference to every view the tokens of `text` name, so a host that emits a token may release its own view once the emit returns.
std::vector<varn::dataview::DataView> JsonSerializer::acquireViews(std::string_view text)
{
    if (text.find(varn::dataview::DataView::kTokenKey) == std::string_view::npos)
    {
        return {};
    }

    const nlohmann::json parsed = nlohmann::json::parse(text, nullptr, false);
    if (parsed.is_discarded())
    {
        throw std::invalid_argument("The JSON of the event is not valid.");
    }

    return varn::dataview::DataViewJson::acquire(parsed);
}

} // namespace varn::json
