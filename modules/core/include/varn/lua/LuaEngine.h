#pragma once

#include <lua.hpp>
#include <string>

namespace varn::runtime
{
class Runtime;
}

namespace varn::lua
{

using LuaPrePcallHook = void (*)(lua_State* L, void* userdata);

class LuaEngine
{
public:
    explicit LuaEngine(varn::runtime::Runtime& runtime);
    ~LuaEngine();

    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    int runFile(const std::string& path);
    int runString(const std::string& source, const std::string& chunkName);
    bool runStringWithoutEventLoop(const std::string& source, const std::string& chunkName, std::string* errorMessage, LuaPrePcallHook prePcallHook = nullptr, void* prePcallUserdata = nullptr);
    lua_State* state();

private:
    static int handlePanic(lua_State* L);

    void installNativeModules();
    void configureArgTable();

    varn::runtime::Runtime& runtime;
    lua_State* L = nullptr;
};

} // namespace varn::lua
