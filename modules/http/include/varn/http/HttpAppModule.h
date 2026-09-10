#pragma once

struct lua_State;

namespace varn::http
{

class HttpAppModule
{
public:
    HttpAppModule() = delete;

    static void registerApp(struct lua_State* L);
};

} // namespace varn::http
