#pragma once

#include <string>
#include <string_view>

struct lua_State;

namespace varn::http
{

class HttpMultipart
{
public:
    static std::string extractBoundary(const std::string& contentType);
    static std::string multipartAttribute(const std::string& headers, const std::string& key);
    static std::string multipartContentType(const std::string& headers);
    static void pushMultipart(lua_State* L, std::string_view body, const std::string& boundary);
};

} // namespace varn::http
