#pragma once

#include <string>
#include <utility>
#include <vector>

struct lua_State;

namespace varn::runtime
{
class Runtime;
}

namespace varn::zip
{

class ZipModule
{
public:
    ZipModule() = delete;
    static void install(lua_State* L);

private:
    static varn::runtime::Runtime& luaRuntime(lua_State* L);

    static void performExtract(const std::string& zipPath, const std::string& destDir);
    static void performCreate(const std::string& zipPath, const std::vector<std::pair<std::string, std::string>>& items);
    static std::vector<std::string> performList(const std::string& zipPath);

    static int luaOpen(lua_State* L);
    static int luaExtract(lua_State* L);
    static int luaCreate(lua_State* L);
    static int luaList(lua_State* L);
};

} // namespace varn::zip
