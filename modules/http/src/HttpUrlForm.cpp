#include "HttpUrlForm.h"

#include "HttpUrl.h"

#include <lua.hpp>

#include <string>

namespace varn::http
{

void HttpUrlForm::pushFormUrlEncoded(lua_State* L, const std::string& body)
{
    lua_newtable(L);

    // bound the field count so a body full of separators cannot drive unbounded parse work
    constexpr int maxFields = 10000;
    int fields = 0;

    std::size_t pos = 0;
    while (pos <= body.size())
    {
        // count every scanned pair so a body full of empty separators still stops at the cap
        if (++fields > maxFields)
        {
            break;
        }

        const std::size_t amp = body.find('&', pos);
        const std::string pair = body.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);

        if (!pair.empty())
        {
            const std::size_t eq = pair.find('=');
            const std::string key = HttpUrl::decode(eq == std::string::npos ? pair : pair.substr(0, eq));
            const std::string value = eq == std::string::npos ? std::string() : HttpUrl::decode(pair.substr(eq + 1));
            // length-safe key so a decoded nul byte cannot truncate the field name
            lua_pushlstring(L, key.data(), key.size());
            lua_pushlstring(L, value.data(), value.size());
            lua_settable(L, -3);
        }

        if (amp == std::string::npos)
        {
            break;
        }

        pos = amp + 1;
    }
}

} // namespace varn::http
