#include "varn/xml/XmlSerializer.h"

#include "PugixmlXmlConvert.h"

#include <pugixml.hpp>

#include <lua.hpp>
#include <sstream>
#include <string>

namespace varn::xml
{

namespace
{
class PugixmlXmlDocument
{
public:
    static void declare(pugi::xml_document& doc)
    {
        pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
    }

    static std::string save(const pugi::xml_document& doc, int indent)
    {
        std::ostringstream out;
        if (indent > 0)
        {
            const std::string pad(static_cast<std::size_t>(indent), ' ');
            doc.save(out, pad.c_str(), pugi::format_indent);
            return out.str();
        }

        doc.save(out, "", pugi::format_raw);
        return out.str();
    }
};
} // namespace

// Writes the fields of a flat table under a `root` element, raising a Lua error that names a field XML cannot carry.
std::string XmlSerializer::serialize(lua_State* L, int index)
{
    pugi::xml_document doc;
    PugixmlXmlDocument::declare(doc);
    pugi::xml_node root = doc.append_child("root");

    if (lua_istable(L, index))
    {
        XmlConvert::buildFields(root, L, index);
    }

    return PugixmlXmlDocument::save(doc, 2);
}

std::string XmlSerializer::encodeNode(lua_State* L, int index, int indent)
{
    pugi::xml_document doc;
    PugixmlXmlDocument::declare(doc);
    XmlConvert::buildElement(doc, L, index, 0);
    return PugixmlXmlDocument::save(doc, indent);
}

bool XmlSerializer::parse(lua_State* L, const std::string& text)
{
    pugi::xml_document doc;
    // The pugixml parser does not load external entities or expand DTD entities.
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
