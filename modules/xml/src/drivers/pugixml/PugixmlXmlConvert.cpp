#include "PugixmlXmlConvert.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace varn::xml
{

void XmlConvert::pushElement(lua_State* L, const pugi::xml_node& node, int depth)
{
    if (depth >= kMaxNodeDepth)
    {
        luaL_error(L, "The XML document nests deeper than %d elements.", kMaxNodeDepth);
    }

    if (lua_checkstack(L, 8) == 0)
    {
        luaL_error(L, "The XML document nests too deeply for the Lua stack.");
    }

    lua_newtable(L);

    lua_pushstring(L, node.name());
    lua_setfield(L, -2, "name");

    pugi::xml_attribute attr = node.first_attribute();
    if (attr)
    {
        lua_newtable(L);
        for (; attr; attr = attr.next_attribute())
        {
            lua_pushstring(L, attr.value());
            lua_setfield(L, -2, attr.name());
        }

        lua_setfield(L, -2, "attributes");
    }

    // Gather direct text and collect child elements into an ordered array.
    std::string text;
    int childCount = 0;
    lua_newtable(L);
    const int childArray = lua_gettop(L);
    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
    {
        const pugi::xml_node_type type = child.type();
        if (type == pugi::node_pcdata || type == pugi::node_cdata)
        {
            text += child.value();
        }
        else if (type == pugi::node_element)
        {
            pushElement(L, child, depth + 1);
            lua_rawseti(L, childArray, ++childCount);
        }
    }

    if (childCount > 0)
    {
        lua_setfield(L, -2, "children");
    }
    else
    {
        lua_pop(L, 1);
    }

    if (!text.empty())
    {
        lua_pushlstring(L, text.data(), text.size());
        lua_setfield(L, -2, "text");
    }
}

void XmlConvert::buildElement(pugi::xml_node parent, lua_State* L, int nodeIndex, int depth)
{
    if (depth >= kMaxNodeDepth)
    {
        luaL_error(L, "The node tree nests deeper than %d elements.", kMaxNodeDepth);
    }

    if (lua_checkstack(L, 8) == 0)
    {
        luaL_error(L, "The node tree nests too deeply for the Lua stack.");
    }

    nodeIndex = lua_absindex(L, nodeIndex);

    lua_getfield(L, nodeIndex, "name");
    if (lua_type(L, -1) != LUA_TSTRING)
    {
        luaL_error(L, "Each node needs a \"name\" that is a string.");
    }

    std::size_t nameLength = 0;
    const char* nameData = lua_tolstring(L, -1, &nameLength);
    const std::string name(nameData, nameLength);
    lua_pop(L, 1);

    if (!validName(name))
    {
        luaL_error(L, "The element name \"%s\" is not a valid XML name.", name.c_str());
    }

    pugi::xml_node el = parent.append_child(name.c_str());

    lua_getfield(L, nodeIndex, "attributes");
    if (!lua_isnil(L, -1))
    {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int attrTable = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, attrTable) != 0)
        {
            const std::string key = keyName(L, -2, "An attribute");
            if (!validName(key))
            {
                luaL_error(L, "The attribute name \"%s\" of the element \"%s\" is not a valid XML name.", key.c_str(), name.c_str());
            }

            const std::string value = valueText(L, -1, "The attribute \"" + key + "\" of the element \"" + name + "\"");
            el.append_attribute(key.c_str()) = value.c_str();
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, nodeIndex, "children");
    if (!lua_isnil(L, -1))
    {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int childArray = lua_gettop(L);
        const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, childArray));
        for (lua_Integer i = 1; i <= count; ++i)
        {
            if (lua_rawgeti(L, childArray, i) != LUA_TTABLE)
            {
                luaL_error(L, "The child %I of the element \"%s\" is a %s, not a node table.", i, name.c_str(), luaL_typename(L, -1));
            }

            buildElement(el, L, lua_gettop(L), depth + 1);
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, nodeIndex, "text");
    if (!lua_isnil(L, -1))
    {
        const std::string text = valueText(L, -1, "The text of the element \"" + name + "\"");
        el.text().set(text.c_str(), text.size());
    }

    lua_pop(L, 1);
}

// Writes each field of a flat table as one element under `parent`, which is how a response turns a table into XML.
void XmlConvert::buildFields(pugi::xml_node parent, lua_State* L, int tableIndex)
{
    tableIndex = lua_absindex(L, tableIndex);

    lua_pushnil(L);
    while (lua_next(L, tableIndex) != 0)
    {
        const std::string key = keyName(L, -2, "A field");
        if (!validName(key))
        {
            luaL_error(L, "The field name \"%s\" is not a valid XML name.", key.c_str());
        }

        const std::string text = valueText(L, -1, "The field \"" + key + "\"");
        pugi::xml_node el = parent.append_child(key.c_str());
        el.text().set(text.c_str(), text.size());
        lua_pop(L, 1);
    }
}

// Answers the string key that names an element or an attribute, and refuses any other key, since no number is an XML name and the text of any other value would be an address.
std::string XmlConvert::keyName(lua_State* L, int index, const char* subject)
{
    const int type = lua_type(L, index);
    if (type != LUA_TSTRING)
    {
        luaL_error(L, "%s has a key of type %s, which XML cannot name.", subject, lua_typename(L, type));
    }

    std::size_t length = 0;
    const char* data = lua_tolstring(L, index, &length);
    return std::string(data, length);
}

// Answers the text of a string, a number or a boolean, and refuses any other value rather than writing a stand-in for it.
std::string XmlConvert::valueText(lua_State* L, int index, const std::string& subject)
{
    switch (lua_type(L, index))
    {
    case LUA_TSTRING:
    case LUA_TNUMBER:
    {
        lua_pushvalue(L, index);
        std::size_t length = 0;
        const char* data = lua_tolstring(L, -1, &length);
        std::string text(data, length);
        lua_pop(L, 1);

        checkText(L, text, subject);
        return text;
    }
    case LUA_TBOOLEAN:
        return lua_toboolean(L, index) != 0 ? "true" : "false";
    default:
        luaL_error(L, "%s is a %s, which XML cannot carry.", subject.c_str(), luaL_typename(L, index));
        return {};
    }
}

// Refuses text that is not UTF-8 or that holds a character XML 1.0 cannot carry, rather than writing a document no parser reads back.
void XmlConvert::checkText(lua_State* L, const std::string& text, const std::string& subject)
{
    std::size_t offset = 0;
    while (offset < text.size())
    {
        char32_t character = 0;
        if (!nextCharacter(text, offset, character))
        {
            luaL_error(L, "%s is not valid UTF-8.", subject.c_str());
        }

        if (!xmlCharacter(character))
        {
            std::ostringstream code;
            code << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << static_cast<std::uint32_t>(character);
            luaL_error(L, "%s holds the character U+%s, which XML cannot carry.", subject.c_str(), code.str().c_str());
        }
    }
}

// Answers whether the text is a name of XML 1.0, a start character followed by name characters.
bool XmlConvert::validName(const std::string& text)
{
    if (text.empty())
    {
        return false;
    }

    std::size_t offset = 0;
    bool first = true;
    while (offset < text.size())
    {
        char32_t character = 0;
        if (!nextCharacter(text, offset, character))
        {
            return false;
        }

        const bool allowed = first ? nameStartCharacter(character) : nameCharacter(character);
        if (!allowed)
        {
            return false;
        }

        first = false;
    }

    return true;
}

// Decodes the UTF-8 character at the offset and moves past it, refusing a truncated, overlong or surrogate sequence and a value past U+10FFFF.
bool XmlConvert::nextCharacter(const std::string& text, std::size_t& offset, char32_t& character)
{
    const auto lead = static_cast<unsigned char>(text[offset]);
    if (lead < 0x80)
    {
        character = lead;
        ++offset;
        return true;
    }

    std::size_t length = 0;
    char32_t minimum = 0;
    if ((lead & 0xE0) == 0xC0)
    {
        length = 2;
        minimum = 0x80;
        character = lead & 0x1F;
    }
    else if ((lead & 0xF0) == 0xE0)
    {
        length = 3;
        minimum = 0x800;
        character = lead & 0x0F;
    }
    else if ((lead & 0xF8) == 0xF0)
    {
        length = 4;
        minimum = 0x10000;
        character = lead & 0x07;
    }
    else
    {
        return false;
    }

    if (text.size() - offset < length)
    {
        return false;
    }

    for (std::size_t i = 1; i < length; ++i)
    {
        const auto continuation = static_cast<unsigned char>(text[offset + i]);
        if ((continuation & 0xC0) != 0x80)
        {
            return false;
        }

        character = (character << 6) | (continuation & 0x3F);
    }

    if (character < minimum || character > 0x10FFFF || (character >= 0xD800 && character <= 0xDFFF))
    {
        return false;
    }

    offset += length;
    return true;
}

// The production Char of XML 1.0.
bool XmlConvert::xmlCharacter(char32_t c)
{
    return c == 0x09 || c == 0x0A || c == 0x0D || (c >= 0x20 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF);
}

// The production NameStartChar of XML 1.0.
bool XmlConvert::nameStartCharacter(char32_t c)
{
    return c == ':' || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8 && c <= 0x2FF) || (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D) || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
}

// The production NameChar of XML 1.0.
bool XmlConvert::nameCharacter(char32_t c)
{
    return nameStartCharacter(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0xB7 || (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040);
}

} // namespace varn::xml
