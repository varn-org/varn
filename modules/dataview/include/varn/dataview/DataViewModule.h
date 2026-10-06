#pragma once

#include "varn/dataview/DataView.h"

struct lua_State;

namespace varn::dataview
{

class DataViewModule
{
public:
    DataViewModule() = delete;

    static constexpr const char* kMetatable = "varn.dataview";

    static void install(lua_State* L);
    static void push(lua_State* L, DataView view);
    static const DataView* test(lua_State* L, int index);
    static const DataView& check(lua_State* L, int index);

private:
    static int luaOpen(lua_State* L);
};

} // namespace varn::dataview
