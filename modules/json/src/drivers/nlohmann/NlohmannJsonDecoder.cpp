#include "NlohmannJsonDecoder.h"

#include "varn/json/JsonSerializer.h"

#include <algorithm>
#include <limits>

namespace varn::json
{

JsonDecoder::JsonDecoder(lua_State* L, int nullIndex, int arrayMetatable)
    : m_state(L)
    , m_nullIndex(nullIndex)
    , m_arrayMetatable(arrayMetatable)
{
}

// Pushes the Lua value of `text` built straight from the parse events, or answers false with a message that names the line and column.
// A null becomes the value at `nullIndex`, or nil when it is zero, and every array carries the array marker so it encodes back as an array.
bool JsonDecoder::decode(lua_State* L, std::string_view text, int nullIndex, std::string& error)
{
    luaL_checkstack(L, 4, nullptr);
    const int base = lua_gettop(L);
    if (nullIndex != 0)
    {
        nullIndex = lua_absindex(L, nullIndex);
    }

    luaL_newmetatable(L, JsonSerializer::arrayMetatable);
    JsonDecoder decoder(L, nullIndex, base + 1);
    if (!nlohmann::json::sax_parse(text.data(), text.data() + text.size(), &decoder))
    {
        lua_settop(L, base);
        error = decoder.failure(text);
        return false;
    }

    lua_remove(L, base + 1);
    return true;
}

bool JsonDecoder::null()
{
    if (m_nullIndex != 0)
    {
        lua_pushvalue(m_state, m_nullIndex);
        return store();
    }

    lua_pushnil(m_state);
    return store();
}

bool JsonDecoder::boolean(bool value)
{
    lua_pushboolean(m_state, value ? 1 : 0);
    return store();
}

bool JsonDecoder::number_integer(std::int64_t value)
{
    lua_pushinteger(m_state, static_cast<lua_Integer>(value));
    return store();
}

// Keeps an integer past the signed range as a float, since it exceeds `lua_Integer`.
bool JsonDecoder::number_unsigned(std::uint64_t value)
{
    if (value > static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max()))
    {
        lua_pushnumber(m_state, static_cast<lua_Number>(value));
        return store();
    }

    lua_pushinteger(m_state, static_cast<lua_Integer>(value));
    return store();
}

bool JsonDecoder::number_float(double value, const std::string& /*text*/)
{
    lua_pushnumber(m_state, static_cast<lua_Number>(value));
    return store();
}

bool JsonDecoder::string(std::string& value)
{
    lua_pushlstring(m_state, value.data(), value.size());
    return store();
}

// Answers false since JSON text carries no binary values.
bool JsonDecoder::binary(nlohmann::json::binary_t& /*value*/)
{
    return false;
}

bool JsonDecoder::start_object(std::size_t /*elements*/)
{
    return open(false);
}

bool JsonDecoder::key(std::string& value)
{
    lua_pushlstring(m_state, value.data(), value.size());
    return true;
}

bool JsonDecoder::end_object()
{
    return close();
}

bool JsonDecoder::start_array(std::size_t /*elements*/)
{
    return open(true);
}

bool JsonDecoder::end_array()
{
    return close();
}

bool JsonDecoder::parse_error(std::size_t position, const std::string& token, const nlohmann::json::exception& ex)
{
    m_errorOffset = position;
    m_errorToken = token;
    m_errorId = ex.id;
    return false;
}

// Opens a table for an array or an object, refusing a level past the depth bound before it can exhaust the stack.
// A table is sized like the last one closed at its depth, since the elements of an array tend to share one shape.
bool JsonDecoder::open(bool array)
{
    const std::size_t depth = m_containers.size();
    if (depth >= maxDepth || lua_checkstack(m_state, 4) == 0)
    {
        m_tooDeep = true;
        return false;
    }

    if (m_hints.size() <= depth)
    {
        m_hints.resize(depth + 1, {0, 0});
    }

    const SizeHint& hint = m_hints[depth];
    lua_createtable(m_state, array ? static_cast<int>(hint.array) : 0, array ? 0 : static_cast<int>(hint.object));
    if (array)
    {
        lua_pushvalue(m_state, m_arrayMetatable);
        lua_setmetatable(m_state, -2);
    }

    m_containers.push_back({array, 0});
    return true;
}

bool JsonDecoder::close()
{
    const Container container = m_containers.back();
    m_containers.pop_back();

    SizeHint& hint = m_hints[m_containers.size()];
    (container.array ? hint.array : hint.object) = std::min<lua_Integer>(container.count, maxHint);
    return store();
}

// Moves the value on top into the open container, after its key for an object, or leaves it as the result at the root.
bool JsonDecoder::store()
{
    if (m_containers.empty())
    {
        return true;
    }

    Container& container = m_containers.back();
    if (container.array)
    {
        lua_rawseti(m_state, -2, ++container.count);
        return true;
    }

    ++container.count;
    lua_rawset(m_state, -3);
    return true;
}

std::string JsonDecoder::failure(std::string_view text) const
{
    if (m_tooDeep)
    {
        return "The input nests deeper than " + std::to_string(maxDepth) + " levels at " + location(text, depthOffset(text)) + ".";
    }

    // Reports the number overflow of nlohmann, whose identifier is 406, by the number it read.
    constexpr int numberOverflow = 406;
    if (m_errorId == numberOverflow)
    {
        return "The number \"" + m_errorToken + "\" at " + location(text, m_errorOffset) + " is out of range.";
    }

    return "The input is not valid JSON at " + location(text, m_errorOffset) + ".";
}

// Answers the count of characters read up to the bracket that opens the first level beyond the depth bound.
std::size_t JsonDecoder::depthOffset(std::string_view text)
{
    std::size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (inString)
        {
            inString = escaped || c != '"';
            escaped = !escaped && c == '\\';
            continue;
        }

        inString = c == '"';
        if ((c == '[' || c == '{') && ++depth > maxDepth)
        {
            return i + 1;
        }

        if ((c == ']' || c == '}') && depth > 0)
        {
            --depth;
        }
    }

    return text.size();
}

// Names the line and the column, counted in bytes from one, of the last character read when `read` characters were read, or of the end of the input.
std::string JsonDecoder::location(std::string_view text, std::size_t read)
{
    const std::size_t index = std::min(read > 0 ? read - 1 : 0, text.size());
    const std::string_view before = text.substr(0, index);
    const std::size_t newline = before.rfind('\n');
    const std::size_t line = 1 + static_cast<std::size_t>(std::count(before.begin(), before.end(), '\n'));
    const std::size_t column = index - (newline == std::string_view::npos ? 0 : newline + 1) + 1;
    return "line " + std::to_string(line) + ", column " + std::to_string(column);
}

} // namespace varn::json
