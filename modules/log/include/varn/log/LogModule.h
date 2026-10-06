#pragma once

#include "varn/log/Log.h"

#include <string>

struct lua_State;

namespace varn::log
{

class LogModule
{
public:
    LogModule() = delete;

    static void install(struct lua_State* L);

private:
    static void emitAt(struct lua_State* L, Level level);
    static void appendValue(struct lua_State* L, int index, std::string& out, int depth, bool quoteStrings);
    static void appendTable(struct lua_State* L, int index, std::string& out, int depth);
    static void appendKey(struct lua_State* L, int index, std::string& out);
    static void appendFields(struct lua_State* L, int index, std::string& out);

    static int luaDebug(struct lua_State* L);
    static int luaInfo(struct lua_State* L);
    static int luaWarn(struct lua_State* L);
    static int luaError(struct lua_State* L);
    static int luaSetLevel(struct lua_State* L);
    static int luaToFile(struct lua_State* L);
    static int luaOpen(struct lua_State* L);
};

} // namespace varn::log
