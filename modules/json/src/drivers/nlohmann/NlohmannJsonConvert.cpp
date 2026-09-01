#include "NlohmannJsonConvert.h"

#include <cstdint>
#include <limits>
#include <string>

namespace varn::json
{

std::string JsonConvert::dumpString(const std::string& value)
{
    return nlohmann::json(value).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool JsonConvert::isSequence(lua_State* L, int index, lua_Integer& length)
{
    length = static_cast<lua_Integer>(lua_rawlen(L, index));
    if (length <= 0)
    {
        return false;
    }

    lua_Integer counted = 0;
    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        ++counted;

        if (!lua_isinteger(L, -2))
        {
            lua_pop(L, 2);
            return false;
        }

        const lua_Integer key = lua_tointeger(L, -2);
        if (key < 1 || key > length)
        {
            lua_pop(L, 2);
            return false;
        }

        lua_pop(L, 1);
    }

    return counted == length;
}

void JsonConvert::pushJson(lua_State* L, const nlohmann::json& value, int depth)
{
    // caps recursion depth above the deserialize pre-scan limit of 200
    constexpr int maxDepth = 256;
    if (depth >= maxDepth || lua_checkstack(L, 4) == 0)
    {
        lua_pushnil(L);
        return;
    }

    if (value.is_object())
    {
        lua_createtable(L, 0, static_cast<int>(value.size()));
        for (auto it = value.begin(); it != value.end(); ++it)
        {
            // pushes the key with its length to preserve an embedded nul
            const std::string& key = it.key();
            lua_pushlstring(L, key.data(), key.size());
            pushJson(L, it.value(), depth + 1);
            lua_rawset(L, -3);
        }

        return;
    }

    if (value.is_array())
    {
        lua_createtable(L, static_cast<int>(value.size()), 0);
        int index = 0;
        for (const auto& element : value)
        {
            pushJson(L, element, depth + 1);
            lua_rawseti(L, -2, ++index);
        }

        return;
    }

    if (value.is_string())
    {
        const std::string& text = value.get_ref<const nlohmann::json::string_t&>();
        lua_pushlstring(L, text.data(), text.size());
        return;
    }

    if (value.is_boolean())
    {
        lua_pushboolean(L, value.get<bool>() ? 1 : 0);
        return;
    }

    if (value.is_number_unsigned())
    {
        const std::uint64_t number = value.get<std::uint64_t>();
        // keeps values past the signed range as a number since they exceed lua_Integer
        if (number > static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max()))
        {
            lua_pushnumber(L, static_cast<lua_Number>(number));
        }
        else
        {
            lua_pushinteger(L, static_cast<lua_Integer>(number));
        }

        return;
    }

    if (value.is_number_integer())
    {
        lua_pushinteger(L, value.get<lua_Integer>());
        return;
    }

    if (value.is_number_float())
    {
        lua_pushnumber(L, value.get<lua_Number>());
        return;
    }

    lua_pushnil(L);
}

} // namespace varn::json
