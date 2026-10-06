#pragma once

#include <lua.hpp>
#include <pugixml.hpp>

namespace varn::xml
{

class XmlConvert
{
public:
    XmlConvert() = delete;

    static void pushElement(lua_State* L, const pugi::xml_node& node, int depth);
    static void buildElement(pugi::xml_node parent, lua_State* L, int nodeIndex, int depth);

private:
    static constexpr int kMaxNodeDepth = 256;
};

} // namespace varn::xml
