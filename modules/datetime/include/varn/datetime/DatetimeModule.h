#pragma once

struct lua_State;

namespace varn::datetime
{

class DatetimeModule
{
public:
    DatetimeModule() = delete;

    static void install(lua_State* L);

private:
    static int luaOpen(lua_State* L);

    static int luaNow(lua_State* L);
    static int luaFromUnix(lua_State* L);
    static int luaFromMillis(lua_State* L);
    static int luaParse(lua_State* L);
    static int luaFromFields(lua_State* L);
    static int luaMin(lua_State* L);
    static int luaMax(lua_State* L);

    static int methodUnix(lua_State* L);
    static int methodMillis(lua_State* L);
    static int methodIso(lua_State* L);
    static int methodFormat(lua_State* L);
    static int methodFields(lua_State* L);

    static int methodYear(lua_State* L);
    static int methodMonth(lua_State* L);
    static int methodDay(lua_State* L);
    static int methodHour(lua_State* L);
    static int methodMinute(lua_State* L);
    static int methodSecond(lua_State* L);
    static int methodWeekday(lua_State* L);
    static int methodYearday(lua_State* L);
    static int methodWeekdayName(lua_State* L);
    static int methodMonthName(lua_State* L);
    static int methodIsLeapYear(lua_State* L);
    static int methodDaysInMonth(lua_State* L);

    static int methodAdd(lua_State* L);
    static int methodSubtract(lua_State* L);
    static int methodDiff(lua_State* L);
    static int methodDiffIn(lua_State* L);
    static int methodStartOf(lua_State* L);
    static int methodEndOf(lua_State* L);

    static int metaToString(lua_State* L);
    static int metaEq(lua_State* L);
    static int metaLt(lua_State* L);
    static int metaLe(lua_State* L);
};

} // namespace varn::datetime
