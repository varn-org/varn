#pragma once

#include <string>

struct lua_State;

namespace varn::http
{

class HttpUrlForm
{
public:
    static void pushFormUrlEncoded(lua_State* L, const std::string& body);
};

} // namespace varn::http
