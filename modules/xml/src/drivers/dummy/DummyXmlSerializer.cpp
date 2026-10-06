#include "varn/xml/XmlSerializer.h"

#include <stdexcept>
#include <string>

namespace varn::xml
{

std::string XmlSerializer::serialize(lua_State* /*L*/, int /*index*/)
{
    throw std::runtime_error("[XmlSerializer] The XML module is not available in this build.");
}

std::string XmlSerializer::encodeNode(lua_State* /*L*/, int /*index*/, int /*indent*/)
{
    throw std::runtime_error("[XmlSerializer] The XML module is not available in this build.");
}

bool XmlSerializer::parse(lua_State* /*L*/, const std::string& /*text*/)
{
    throw std::runtime_error("[XmlSerializer] The XML module is not available in this build.");
}

std::string XmlSerializer::sanitizeElementName(const std::string& /*raw*/)
{
    throw std::runtime_error("[XmlSerializer] The XML module is not available in this build.");
}

} // namespace varn::xml
