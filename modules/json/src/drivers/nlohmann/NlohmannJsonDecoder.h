#pragma once

#include <lua.hpp>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace varn::json
{

class JsonDecoder
{
public:
    static bool decode(lua_State* L, std::string_view text, int nullIndex, std::string& error);

    bool null();
    bool boolean(bool value);
    bool number_integer(std::int64_t value);
    bool number_unsigned(std::uint64_t value);
    bool number_float(double value, const std::string& text);
    bool string(std::string& value);
    bool binary(nlohmann::json::binary_t& value);
    bool start_object(std::size_t elements);
    bool key(std::string& value);
    bool end_object();
    bool start_array(std::size_t elements);
    bool end_array();
    bool parse_error(std::size_t position, const std::string& token, const nlohmann::json::exception& ex);

private:
    struct Container
    {
        bool array;
        lua_Integer count;
    };

    struct SizeHint
    {
        lua_Integer array;
        lua_Integer object;
    };

    JsonDecoder(lua_State* L, int nullIndex, int arrayMetatable);

    bool open(bool array);
    bool close();
    bool store();
    std::string failure(std::string_view text) const;

    static std::size_t depthOffset(std::string_view text);
    static std::string location(std::string_view text, std::size_t read);

    static constexpr std::size_t maxDepth = 200;
    static constexpr lua_Integer maxHint = 1 << 16;

    lua_State* m_state;
    int m_nullIndex;
    int m_arrayMetatable;
    std::vector<Container> m_containers;
    std::vector<SizeHint> m_hints;
    bool m_tooDeep = false;
    std::size_t m_errorOffset = 0;
    std::string m_errorToken;
    int m_errorId = 0;
};

} // namespace varn::json
