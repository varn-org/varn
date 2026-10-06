#pragma once

namespace varn::runtime
{
class Runtime;
}

struct lua_State;

namespace varn::http::client
{

/// A Lua function a request calls later, held in the registry until the loop releases it, whatever thread drops the last owner.
class HttpClientLuaFunction
{
public:
    HttpClientLuaFunction(varn::runtime::Runtime& runtime, int ref);
    ~HttpClientLuaFunction();

    HttpClientLuaFunction(const HttpClientLuaFunction&) = delete;
    HttpClientLuaFunction& operator=(const HttpClientLuaFunction&) = delete;

    void push(lua_State* L) const;

private:
    varn::runtime::Runtime& runtime;
    int ref;
};

} // namespace varn::http::client
