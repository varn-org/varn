#pragma once

#include <lua.hpp>
#include <string>

namespace varn::xml
{

class XmlSerializer
{
public:
    XmlSerializer() = delete;

    static std::string serialize(lua_State* L, int index);
    static std::string encodeNode(lua_State* L, int index, int indent);
    static bool parse(lua_State* L, const std::string& text);
};

} // namespace varn::xml
