#pragma once

#include "varn/http/HttpClientPerform.h"

#include <lua.hpp>

#include <cctype>
#include <string>

namespace varn::http::client
{

class HttpClientResponseLua
{
public:
    HttpClientResponseLua() = delete;

    static void pushHeaders(lua_State* L, const ResponseHeaders& headers)
    {
        lua_createtable(L, 0, static_cast<int>(headers.size()));
        for (const auto& [name, value] : headers)
        {
            // lowercase the name so a caller can look the header up case-insensitively
            std::string key = name;
            for (char& c : key)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            lua_pushlstring(L, value.data(), value.size());
            lua_setfield(L, -2, key.c_str());
        }
    }

    static void pushResponse(lua_State* L, int status, const ResponseHeaders& headers, const std::string& body)
    {
        lua_createtable(L, 0, 3);

        lua_pushinteger(L, status);
        lua_setfield(L, -2, "status");

        pushHeaders(L, headers);
        lua_setfield(L, -2, "headers");

        lua_pushlstring(L, body.data(), body.size());
        lua_setfield(L, -2, "body");
    }
};

} // namespace varn::http::client
