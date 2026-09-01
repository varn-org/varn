#include "varn/xml/XmlSerializer.h"

#include "PugixmlXmlConvert.h"

#include <pugixml.hpp>

#include <cctype>
#include <lua.hpp>
#include <sstream>
#include <string>

namespace varn::xml
{

std::string XmlSerializer::sanitizeElementName(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw)
    {
        // keep ascii name characters and pass utf-8 bytes through so non-ascii names are not mangled
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || static_cast<unsigned char>(c) >= 0x80)
        {
            out += c;
        }
        else
        {
            out += '_';
        }
    }

    if (out.empty())
    {
        return "item";
    }

    if (std::isdigit(static_cast<unsigned char>(out[0])))
    {
        out.insert(out.begin(), 'n');
    }

    return out;
}

std::string XmlSerializer::sanitizeText(const char* data, std::size_t size)
{
    std::string out;
    out.reserve(size);
    for (std::size_t i = 0; i < size; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(data[i]);

        // drop the control characters xml 1.0 forbids so the emitted text stays well-formed and round-trips
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)
        {
            continue;
        }

        out += static_cast<char>(c);
    }

    return out;
}

std::string XmlSerializer::serialize(lua_State* L, int index)
{
    index = lua_absindex(L, index);
    pugi::xml_document doc;
    pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";
    pugi::xml_node root = doc.append_child("root");

    if (!lua_istable(L, index))
    {
        std::ostringstream o;
        doc.save(o, "  ", pugi::format_indent);
        return o.str();
    }

    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        // copy the key with luaL_tolstring and drop the copy before reading the value at -1
        const char* keyRaw = luaL_tolstring(L, -2, nullptr);
        const std::string key = keyRaw ? keyRaw : "";
        lua_pop(L, 1);

        const std::string tag = sanitizeElementName(key);
        pugi::xml_node el = root.append_child(tag.c_str());

        if (lua_isstring(L, -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            const std::string text = sanitizeText(s, len);
            el.text().set(text.c_str(), text.size());
        }
        else if (lua_isboolean(L, -1))
        {
            el.append_child(pugi::node_pcdata).set_value(lua_toboolean(L, -1) ? "true" : "false");
        }
        else if (lua_isnil(L, -1))
        {
            el.append_child(pugi::node_pcdata).set_value("null");
        }
        else
        {
            el.append_child(pugi::node_pcdata).set_value("[unsupported]");
        }

        lua_pop(L, 1);
    }

    std::ostringstream out;
    doc.save(out, "  ", pugi::format_indent);
    return out.str();
}

std::string XmlSerializer::encodeNode(lua_State* L, int index, int indent)
{
    index = lua_absindex(L, index);

    pugi::xml_document doc;
    pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    if (lua_istable(L, index))
    {
        XmlConvert::buildElement(doc, L, index, 0);
    }

    std::ostringstream out;
    if (indent > 0)
    {
        const std::string pad(static_cast<std::size_t>(indent), ' ');
        doc.save(out, pad.c_str(), pugi::format_indent);
    }
    else
    {
        doc.save(out, "", pugi::format_raw);
    }

    return out.str();
}

bool XmlSerializer::parse(lua_State* L, const std::string& text)
{
    pugi::xml_document doc;
    // pugixml does not load external entities or expand DTD entities
    const pugi::xml_parse_result result = doc.load_buffer(text.data(), text.size(), pugi::parse_default);
    if (!result)
    {
        return false;
    }

    pugi::xml_node root = doc.first_child();
    while (root && root.type() != pugi::node_element)
    {
        root = root.next_sibling();
    }

    if (!root)
    {
        return false;
    }

    XmlConvert::pushElement(L, root, 0);
    return true;
}

} // namespace varn::xml
