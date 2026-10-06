#pragma once

#include <string_view>

struct lua_State;

namespace varn::http
{

class HttpUrlForm
{
public:
    static void pushFormUrlEncoded(lua_State* L, std::string_view body);
};

} // namespace varn::http
