#include "PugixmlXmlConvert.h"

#include "varn/xml/XmlSerializer.h"

#include <string>

namespace varn::xml
{

void XmlConvert::pushElement(lua_State* L, const pugi::xml_node& node, int depth)
{
    if (depth >= kMaxNodeDepth || lua_checkstack(L, 8) == 0)
    {
        lua_pushnil(L);
        return;
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

    // gather direct text and collect child elements into an ordered array
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
    if (depth >= kMaxNodeDepth || lua_checkstack(L, 8) == 0)
    {
        return;
    }

    nodeIndex = lua_absindex(L, nodeIndex);

    std::string name = "node";
    lua_getfield(L, nodeIndex, "name");
    if (lua_isstring(L, -1))
    {
        name = lua_tostring(L, -1);
    }

    lua_pop(L, 1);

    pugi::xml_node el = parent.append_child(XmlSerializer::sanitizeElementName(name).c_str());

    lua_getfield(L, nodeIndex, "attributes");
    if (lua_istable(L, -1))
    {
        const int attrTable = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, attrTable) != 0)
        {
            const char* keyRaw = luaL_tolstring(L, -2, nullptr);
            const std::string key = keyRaw ? keyRaw : "";
            lua_pop(L, 1);
            const char* value = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
            el.append_attribute(XmlSerializer::sanitizeElementName(key).c_str()) = value;
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, nodeIndex, "children");
    if (lua_istable(L, -1))
    {
        const int childArray = lua_gettop(L);
        const int count = static_cast<int>(lua_rawlen(L, childArray));
        for (int i = 1; i <= count; ++i)
        {
            lua_rawgeti(L, childArray, i);
            if (lua_istable(L, -1))
            {
                buildElement(el, L, lua_gettop(L), depth + 1);
            }

            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);

    lua_getfield(L, nodeIndex, "text");
    if (lua_isstring(L, -1))
    {
        size_t len = 0;
        const char* s = lua_tolstring(L, -1, &len);
        const std::string text = XmlSerializer::sanitizeText(s, len);
        el.text().set(text.c_str(), text.size());
    }

    lua_pop(L, 1);
}

} // namespace varn::xml
