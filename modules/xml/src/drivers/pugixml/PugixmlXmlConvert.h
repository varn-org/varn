#pragma once

#include <lua.hpp>
#include <pugixml.hpp>

#include <cstddef>
#include <string>

namespace varn::xml
{

class XmlConvert
{
public:
    XmlConvert() = delete;

    static void pushElement(lua_State* L, const pugi::xml_node& node, int depth);
    static void buildElement(pugi::xml_node parent, lua_State* L, int nodeIndex, int depth);
    static void buildFields(pugi::xml_node parent, lua_State* L, int tableIndex);

private:
    static std::string keyName(lua_State* L, int index, const char* subject);
    static std::string valueText(lua_State* L, int index, const std::string& subject);
    static void checkText(lua_State* L, const std::string& text, const std::string& subject);
    static bool validName(const std::string& text);
    static bool nextCharacter(const std::string& text, std::size_t& offset, char32_t& character);
    static bool xmlCharacter(char32_t c);
    static bool nameStartCharacter(char32_t c);
    static bool nameCharacter(char32_t c);

    static constexpr int kMaxNodeDepth = 256;
};

} // namespace varn::xml
