#include "varn/datetime/DatetimeModule.h"

#include <date/date.h>
#include <lua.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace varn::datetime
{

namespace
{
constexpr char kDatetimeMeta[] = "varn.datetime";

using Millis = std::chrono::milliseconds;
using TimePoint = date::sys_time<Millis>;

class DatetimeHelpers
{
public:
    static int64_t check(lua_State* L, int index)
    {
        return *static_cast<int64_t*>(luaL_checkudata(L, index, kDatetimeMeta));
    }

    static void push(lua_State* L, int64_t millis)
    {
        void* memory = lua_newuserdatauv(L, sizeof(int64_t), 0);
        *static_cast<int64_t*>(memory) = millis;

        luaL_getmetatable(L, kDatetimeMeta);
        lua_setmetatable(L, -2);
    }

    static TimePoint toTimePoint(int64_t millis)
    {
        return TimePoint{Millis{millis}};
    }

    static date::year_month_day ymdOf(int64_t millis)
    {
        return date::year_month_day{std::chrono::floor<date::days>(DatetimeHelpers::toTimePoint(millis))};
    }

    static date::hh_mm_ss<Millis> todOf(int64_t millis)
    {
        const TimePoint tp = DatetimeHelpers::toTimePoint(millis);
        return date::hh_mm_ss<Millis>{tp - std::chrono::floor<date::days>(tp)};
    }

    static std::string twoDigits(int value)
    {
        std::string out = std::to_string(value);
        if (out.size() < 2)
        {
            out.insert(out.begin(), '0');
        }

        return out;
    }

    static std::string offsetSuffix(int offsetMinutes)
    {
        if (offsetMinutes == 0)
        {
            return "Z";
        }

        const char sign = offsetMinutes < 0 ? '-' : '+';
        const int magnitude = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;

        return std::string(1, sign) + DatetimeHelpers::twoDigits(magnitude / 60) + ":" + DatetimeHelpers::twoDigits(magnitude % 60);
    }

    static std::string formatIso(int64_t millis, int offsetMinutes)
    {
        const TimePoint shifted = DatetimeHelpers::toTimePoint(millis) + std::chrono::minutes{offsetMinutes};
        const std::string suffix = DatetimeHelpers::offsetSuffix(offsetMinutes);

        if (millis % 1000 == 0)
        {
            return date::format("%Y-%m-%dT%H:%M:%S", std::chrono::floor<std::chrono::seconds>(shifted)) + suffix;
        }

        return date::format("%Y-%m-%dT%H:%M:%S", shifted) + suffix;
    }

    static void pushFields(lua_State* L, int64_t millis, int offsetMinutes)
    {
        const TimePoint shifted = DatetimeHelpers::toTimePoint(millis) + std::chrono::minutes{offsetMinutes};
        const date::sys_days dp = std::chrono::floor<date::days>(shifted);
        const date::year_month_day ymd{dp};
        const date::hh_mm_ss<Millis> tod{shifted - dp};
        const date::weekday weekday{dp};
        const date::sys_days yearStart{ymd.year() / date::January / 1};

        lua_createtable(L, 0, 10);

        lua_pushinteger(L, static_cast<int>(ymd.year()));
        lua_setfield(L, -2, "year");
        lua_pushinteger(L, static_cast<unsigned>(ymd.month()));
        lua_setfield(L, -2, "month");
        lua_pushinteger(L, static_cast<unsigned>(ymd.day()));
        lua_setfield(L, -2, "day");
        lua_pushinteger(L, tod.hours().count());
        lua_setfield(L, -2, "hour");
        lua_pushinteger(L, tod.minutes().count());
        lua_setfield(L, -2, "minute");
        lua_pushinteger(L, tod.seconds().count());
        lua_setfield(L, -2, "second");
        lua_pushinteger(L, tod.subseconds().count());
        lua_setfield(L, -2, "millis");
        lua_pushinteger(L, weekday.iso_encoding());
        lua_setfield(L, -2, "weekday");
        lua_pushinteger(L, (dp - yearStart).count() + 1);
        lua_setfield(L, -2, "yearday");
        lua_pushinteger(L, offsetMinutes);
        lua_setfield(L, -2, "offset");
    }

    static int64_t shiftInstant(int64_t millis, int years, int months, long long days, long long hours, long long minutes, long long seconds, long long millisDelta)
    {
        const TimePoint tp = DatetimeHelpers::toTimePoint(millis);
        const date::sys_days dp = std::chrono::floor<date::days>(tp);
        const Millis timeOfDay = tp - dp;

        const date::year_month_day ymd = date::year_month_day{dp} + date::years{years} + date::months{months};

        // snaps to the last day when the shifted month is shorter
        const date::sys_days shiftedDay = ymd.ok() ? date::sys_days{ymd} : date::sys_days{ymd.year() / ymd.month() / date::last};

        const TimePoint result = shiftedDay + timeOfDay + date::days{static_cast<int>(days)} + std::chrono::hours{hours} + std::chrono::minutes{minutes} + std::chrono::seconds{seconds} + Millis{millisDelta};

        return result.time_since_epoch().count();
    }

    static int compareInstant(int64_t a, int64_t b)
    {
        if (a < b)
        {
            return -1;
        }

        if (a > b)
        {
            return 1;
        }

        return 0;
    }

    static long long calendarUnitsBetween(int64_t self, int64_t other, bool years)
    {
        const int sign = DatetimeHelpers::compareInstant(self, other);
        if (sign == 0)
        {
            return 0;
        }

        const date::year_month_day s = DatetimeHelpers::ymdOf(self);
        const date::year_month_day o = DatetimeHelpers::ymdOf(other);

        const long long rawMonths = (static_cast<long long>(static_cast<int>(s.year())) - static_cast<int>(o.year())) * 12 + (static_cast<int>(static_cast<unsigned>(s.month())) - static_cast<int>(static_cast<unsigned>(o.month())));
        long long diff = years ? std::llabs(static_cast<long long>(static_cast<int>(s.year())) - static_cast<int>(o.year())) : std::llabs(rawMonths);

        const int64_t moved = years ? DatetimeHelpers::shiftInstant(self, static_cast<int>(-sign * diff), 0, 0, 0, 0, 0, 0) : DatetimeHelpers::shiftInstant(self, 0, static_cast<int>(-sign * diff), 0, 0, 0, 0, 0);

        if (DatetimeHelpers::compareInstant(moved, other) == -sign)
        {
            diff -= 1;
        }

        return sign * diff;
    }

    static bool unitBounds(std::string_view unit, int64_t millis, TimePoint& start, TimePoint& next)
    {
        const TimePoint tp = DatetimeHelpers::toTimePoint(millis);
        const date::sys_days dp = std::chrono::floor<date::days>(tp);
        const date::year_month_day ymd{dp};

        if (unit == "year")
        {
            start = date::sys_days{ymd.year() / date::January / 1};
            next = date::sys_days{(ymd.year() + date::years{1}) / date::January / 1};
            return true;
        }
        if (unit == "month")
        {
            const date::year_month ym = ymd.year() / ymd.month();
            start = date::sys_days{ym / 1};
            next = date::sys_days{(ym + date::months{1}) / 1};
            return true;
        }
        if (unit == "week")
        {
            const date::sys_days weekStart = dp - (date::weekday{dp} - date::Monday);
            start = weekStart;
            next = weekStart + date::days{7};
            return true;
        }
        if (unit == "day")
        {
            start = dp;
            next = dp + date::days{1};
            return true;
        }
        if (unit == "hour")
        {
            start = std::chrono::floor<std::chrono::hours>(tp);
            next = std::chrono::floor<std::chrono::hours>(tp) + std::chrono::hours{1};
            return true;
        }
        if (unit == "minute")
        {
            start = std::chrono::floor<std::chrono::minutes>(tp);
            next = std::chrono::floor<std::chrono::minutes>(tp) + std::chrono::minutes{1};
            return true;
        }
        if (unit == "second")
        {
            start = std::chrono::floor<std::chrono::seconds>(tp);
            next = std::chrono::floor<std::chrono::seconds>(tp) + std::chrono::seconds{1};
            return true;
        }

        return false;
    }

    static bool readDigits(std::string_view text, std::size_t& cursor, int count, int& out)
    {
        if (cursor + count > text.size())
        {
            return false;
        }

        int value = 0;
        for (int i = 0; i < count; ++i)
        {
            const char c = text[cursor + i];
            if (c < '0' || c > '9')
            {
                return false;
            }

            value = value * 10 + (c - '0');
        }

        cursor += count;
        out = value;
        return true;
    }

    static std::optional<int64_t> parseIso(std::string_view text)
    {
        std::size_t begin = 0;
        std::size_t end = text.size();
        while (begin < end && (text[begin] == ' ' || text[begin] == '\t'))
        {
            ++begin;
        }
        while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t'))
        {
            --end;
        }

        const std::string_view s = text.substr(begin, end - begin);
        std::size_t cursor = 0;

        int year = 0;
        int month = 0;
        int day = 0;
        if (!DatetimeHelpers::readDigits(s, cursor, 4, year) || cursor >= s.size() || s[cursor++] != '-')
        {
            return std::nullopt;
        }
        if (!DatetimeHelpers::readDigits(s, cursor, 2, month) || cursor >= s.size() || s[cursor++] != '-')
        {
            return std::nullopt;
        }
        if (!DatetimeHelpers::readDigits(s, cursor, 2, day))
        {
            return std::nullopt;
        }

        int hour = 0;
        int minute = 0;
        int second = 0;
        int millisOfSecond = 0;
        if (cursor < s.size() && (s[cursor] == 'T' || s[cursor] == ' '))
        {
            ++cursor;
            if (!DatetimeHelpers::readDigits(s, cursor, 2, hour) || cursor >= s.size() || s[cursor++] != ':')
            {
                return std::nullopt;
            }
            if (!DatetimeHelpers::readDigits(s, cursor, 2, minute) || cursor >= s.size() || s[cursor++] != ':')
            {
                return std::nullopt;
            }
            if (!DatetimeHelpers::readDigits(s, cursor, 2, second))
            {
                return std::nullopt;
            }

            if (cursor < s.size() && s[cursor] == '.')
            {
                ++cursor;
                int place = 100;
                bool any = false;
                while (cursor < s.size() && s[cursor] >= '0' && s[cursor] <= '9')
                {
                    if (place >= 1)
                    {
                        millisOfSecond += (s[cursor] - '0') * place;
                        place /= 10;
                    }
                    ++cursor;
                    any = true;
                }
                if (!any)
                {
                    return std::nullopt;
                }
            }
        }

        int offsetMinutes = 0;
        if (cursor < s.size())
        {
            const char marker = s[cursor];
            if (marker == 'Z' || marker == 'z')
            {
                ++cursor;
            }
            else if (marker == '+' || marker == '-')
            {
                ++cursor;
                int offHour = 0;
                int offMin = 0;
                if (!DatetimeHelpers::readDigits(s, cursor, 2, offHour))
                {
                    return std::nullopt;
                }
                if (cursor < s.size() && s[cursor] == ':')
                {
                    ++cursor;
                }
                if (!DatetimeHelpers::readDigits(s, cursor, 2, offMin))
                {
                    return std::nullopt;
                }

                offsetMinutes = (offHour * 60 + offMin) * (marker == '-' ? -1 : 1);
            }
            else
            {
                return std::nullopt;
            }
        }

        if (cursor != s.size())
        {
            return std::nullopt;
        }

        const date::year_month_day ymd{date::year{year}, date::month{static_cast<unsigned>(month)}, date::day{static_cast<unsigned>(day)}};
        if (!ymd.ok())
        {
            return std::nullopt;
        }

        const TimePoint local = date::sys_days{ymd} + std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second} + Millis{millisOfSecond};

        const TimePoint utc = local - std::chrono::minutes{offsetMinutes};
        return utc.time_since_epoch().count();
    }

    static long long fieldOr(lua_State* L, int table, const char* key, long long fallback)
    {
        lua_getfield(L, table, key);
        const long long value = luaL_optinteger(L, -1, fallback);
        lua_pop(L, 1);
        return value;
    }

    static int64_t shiftByDeltaTable(lua_State* L, int64_t millis, int table, int sign)
    {
        const long long years = DatetimeHelpers::fieldOr(L, table, "years", 0);
        const long long months = DatetimeHelpers::fieldOr(L, table, "months", 0);
        const long long weeks = DatetimeHelpers::fieldOr(L, table, "weeks", 0);
        const long long days = DatetimeHelpers::fieldOr(L, table, "days", 0);
        const long long hours = DatetimeHelpers::fieldOr(L, table, "hours", 0);
        const long long minutes = DatetimeHelpers::fieldOr(L, table, "minutes", 0);
        const long long seconds = DatetimeHelpers::fieldOr(L, table, "seconds", 0);
        const long long millisDelta = DatetimeHelpers::fieldOr(L, table, "millis", 0);

        const long long shiftYears = sign * years;
        const long long shiftMonths = sign * months;
        const long long shiftDays = sign * (weeks * 7 + days);

        // the year, month and day deltas flow through int-based calendar types, so reject values that would overflow the cast
        constexpr long long kMinInt = std::numeric_limits<int>::min();
        constexpr long long kMaxInt = std::numeric_limits<int>::max();
        if (shiftYears < kMinInt || shiftYears > kMaxInt || shiftMonths < kMinInt || shiftMonths > kMaxInt || shiftDays < kMinInt || shiftDays > kMaxInt)
        {
            return luaL_error(L, "[Datetime] A year, month, week or day delta is out of range.");
        }

        // the sub-day deltas are summed into the millisecond instant, so bound each so its span cannot overflow the int64 rep
        constexpr long long kMaxSpanMs = 1000000000000000000LL;
        if (hours > kMaxSpanMs / 3600000 || hours < -(kMaxSpanMs / 3600000) || minutes > kMaxSpanMs / 60000 || minutes < -(kMaxSpanMs / 60000) || seconds > kMaxSpanMs / 1000 || seconds < -(kMaxSpanMs / 1000) || millisDelta > kMaxSpanMs || millisDelta < -kMaxSpanMs)
        {
            return luaL_error(L, "[Datetime] An hour, minute, second or millisecond delta is out of range.");
        }

        return DatetimeHelpers::shiftInstant(millis, static_cast<int>(shiftYears), static_cast<int>(shiftMonths), shiftDays, sign * hours, sign * minutes, sign * seconds, sign * millisDelta);
    }

    static long long requiredField(lua_State* L, int table, const char* key)
    {
        lua_getfield(L, table, key);
        if (lua_isnil(L, -1))
        {
            luaL_error(L, "[Datetime] fromFields requires the '%s' field", key);
        }

        const long long value = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
        return value;
    }
};
} // namespace

int DatetimeModule::luaNow(lua_State* L)
{
    const auto now = std::chrono::floor<Millis>(std::chrono::system_clock::now());
    DatetimeHelpers::push(L, now.time_since_epoch().count());
    return 1;
}

int DatetimeModule::luaFromUnix(lua_State* L)
{
    const lua_Number seconds = luaL_checknumber(L, 1);
    DatetimeHelpers::push(L, static_cast<int64_t>(std::llround(seconds * 1000.0)));
    return 1;
}

int DatetimeModule::luaFromMillis(lua_State* L)
{
    DatetimeHelpers::push(L, static_cast<int64_t>(luaL_checkinteger(L, 1)));
    return 1;
}

int DatetimeModule::luaParse(lua_State* L)
{
    std::size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);

    const std::optional<int64_t> millis = DatetimeHelpers::parseIso(std::string_view(data, len));
    if (!millis)
    {
        return luaL_error(L, "[Datetime] Not an ISO-8601 datetime: %s", data);
    }

    DatetimeHelpers::push(L, *millis);
    return 1;
}

int DatetimeModule::luaFromFields(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    const int year = static_cast<int>(DatetimeHelpers::requiredField(L, 1, "year"));
    const int month = static_cast<int>(DatetimeHelpers::requiredField(L, 1, "month"));
    const int day = static_cast<int>(DatetimeHelpers::requiredField(L, 1, "day"));
    const long long hour = DatetimeHelpers::fieldOr(L, 1, "hour", 0);
    const long long minute = DatetimeHelpers::fieldOr(L, 1, "minute", 0);
    const long long second = DatetimeHelpers::fieldOr(L, 1, "second", 0);
    const long long millisOfSecond = DatetimeHelpers::fieldOr(L, 1, "millis", 0);
    const long long offsetMinutes = DatetimeHelpers::fieldOr(L, 1, "offset", 0);

    const date::year_month_day ymd{date::year{year}, date::month{static_cast<unsigned>(month)}, date::day{static_cast<unsigned>(day)}};
    if (!ymd.ok())
    {
        return luaL_error(L, "[Datetime] Not a valid calendar date");
    }

    const TimePoint local = date::sys_days{ymd} + std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second} + Millis{millisOfSecond};

    const TimePoint utc = local - std::chrono::minutes{offsetMinutes};
    DatetimeHelpers::push(L, utc.time_since_epoch().count());
    return 1;
}

int DatetimeModule::luaMin(lua_State* L)
{
    const int count = lua_gettop(L);
    if (count < 1)
    {
        return luaL_error(L, "[Datetime] min requires at least one datetime");
    }

    int64_t best = DatetimeHelpers::check(L, 1);
    for (int i = 2; i <= count; ++i)
    {
        const int64_t value = DatetimeHelpers::check(L, i);
        if (value < best)
        {
            best = value;
        }
    }

    DatetimeHelpers::push(L, best);
    return 1;
}

int DatetimeModule::luaMax(lua_State* L)
{
    const int count = lua_gettop(L);
    if (count < 1)
    {
        return luaL_error(L, "[Datetime] max requires at least one datetime");
    }

    int64_t best = DatetimeHelpers::check(L, 1);
    for (int i = 2; i <= count; ++i)
    {
        const int64_t value = DatetimeHelpers::check(L, i);
        if (value > best)
        {
            best = value;
        }
    }

    DatetimeHelpers::push(L, best);
    return 1;
}

int DatetimeModule::methodUnix(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(std::chrono::floor<std::chrono::seconds>(DatetimeHelpers::toTimePoint(millis)).time_since_epoch().count()));
    return 1;
}

int DatetimeModule::methodMillis(lua_State* L)
{
    lua_pushinteger(L, static_cast<lua_Integer>(DatetimeHelpers::check(L, 1)));
    return 1;
}

int DatetimeModule::methodIso(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const int offsetMinutes = static_cast<int>(luaL_optinteger(L, 2, 0));

    const std::string out = DatetimeHelpers::formatIso(millis, offsetMinutes);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int DatetimeModule::methodFormat(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const char* fmt = luaL_optstring(L, 2, "%Y-%m-%dT%H:%M:%SZ");
    const int offsetMinutes = static_cast<int>(luaL_optinteger(L, 3, 0));

    const TimePoint shifted = DatetimeHelpers::toTimePoint(millis) + std::chrono::minutes{offsetMinutes};
    const std::string out = date::format(fmt, std::chrono::floor<std::chrono::seconds>(shifted));
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int DatetimeModule::methodFields(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const int offsetMinutes = static_cast<int>(luaL_optinteger(L, 2, 0));
    DatetimeHelpers::pushFields(L, millis, offsetMinutes);
    return 1;
}

int DatetimeModule::methodYear(lua_State* L)
{
    lua_pushinteger(L, static_cast<int>(DatetimeHelpers::ymdOf(DatetimeHelpers::check(L, 1)).year()));
    return 1;
}

int DatetimeModule::methodMonth(lua_State* L)
{
    lua_pushinteger(L, static_cast<unsigned>(DatetimeHelpers::ymdOf(DatetimeHelpers::check(L, 1)).month()));
    return 1;
}

int DatetimeModule::methodDay(lua_State* L)
{
    lua_pushinteger(L, static_cast<unsigned>(DatetimeHelpers::ymdOf(DatetimeHelpers::check(L, 1)).day()));
    return 1;
}

int DatetimeModule::methodHour(lua_State* L)
{
    lua_pushinteger(L, DatetimeHelpers::todOf(DatetimeHelpers::check(L, 1)).hours().count());
    return 1;
}

int DatetimeModule::methodMinute(lua_State* L)
{
    lua_pushinteger(L, DatetimeHelpers::todOf(DatetimeHelpers::check(L, 1)).minutes().count());
    return 1;
}

int DatetimeModule::methodSecond(lua_State* L)
{
    lua_pushinteger(L, DatetimeHelpers::todOf(DatetimeHelpers::check(L, 1)).seconds().count());
    return 1;
}

int DatetimeModule::methodWeekday(lua_State* L)
{
    const date::sys_days dp = std::chrono::floor<date::days>(DatetimeHelpers::toTimePoint(DatetimeHelpers::check(L, 1)));
    lua_pushinteger(L, date::weekday{dp}.iso_encoding());
    return 1;
}

int DatetimeModule::methodYearday(lua_State* L)
{
    const date::sys_days dp = std::chrono::floor<date::days>(DatetimeHelpers::toTimePoint(DatetimeHelpers::check(L, 1)));
    const date::year_month_day ymd{dp};
    const date::sys_days yearStart{ymd.year() / date::January / 1};
    lua_pushinteger(L, (dp - yearStart).count() + 1);
    return 1;
}

int DatetimeModule::methodWeekdayName(lua_State* L)
{
    const date::sys_days dp = std::chrono::floor<date::days>(DatetimeHelpers::toTimePoint(DatetimeHelpers::check(L, 1)));
    const std::string out = date::format("%A", dp);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int DatetimeModule::methodMonthName(lua_State* L)
{
    const date::sys_days dp = std::chrono::floor<date::days>(DatetimeHelpers::toTimePoint(DatetimeHelpers::check(L, 1)));
    const std::string out = date::format("%B", dp);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int DatetimeModule::methodIsLeapYear(lua_State* L)
{
    lua_pushboolean(L, DatetimeHelpers::ymdOf(DatetimeHelpers::check(L, 1)).year().is_leap());
    return 1;
}

int DatetimeModule::methodDaysInMonth(lua_State* L)
{
    const date::year_month_day ymd = DatetimeHelpers::ymdOf(DatetimeHelpers::check(L, 1));
    const date::year_month_day_last last{ymd.year() / ymd.month() / date::last};
    lua_pushinteger(L, static_cast<unsigned>(last.day()));
    return 1;
}

int DatetimeModule::methodAdd(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    DatetimeHelpers::push(L, DatetimeHelpers::shiftByDeltaTable(L, millis, 2, 1));
    return 1;
}

int DatetimeModule::methodSubtract(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    DatetimeHelpers::push(L, DatetimeHelpers::shiftByDeltaTable(L, millis, 2, -1));
    return 1;
}

int DatetimeModule::methodDiff(lua_State* L)
{
    const int64_t self = DatetimeHelpers::check(L, 1);
    const int64_t other = DatetimeHelpers::check(L, 2);
    lua_pushinteger(L, static_cast<lua_Integer>((self - other) / 1000));
    return 1;
}

int DatetimeModule::methodDiffIn(lua_State* L)
{
    const int64_t self = DatetimeHelpers::check(L, 1);
    const int64_t other = DatetimeHelpers::check(L, 2);
    const std::string_view unit = luaL_checkstring(L, 3);

    const int64_t delta = self - other;

    if (unit == "millis")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(delta));
        return 1;
    }
    if (unit == "seconds")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(delta / 1000));
        return 1;
    }
    if (unit == "minutes")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(delta / 60000));
        return 1;
    }
    if (unit == "hours")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(delta / 3600000));
        return 1;
    }
    if (unit == "days")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(delta / 86400000));
        return 1;
    }
    if (unit == "weeks")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(delta / 604800000));
        return 1;
    }
    if (unit == "months")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(DatetimeHelpers::calendarUnitsBetween(self, other, false)));
        return 1;
    }
    if (unit == "years")
    {
        lua_pushinteger(L, static_cast<lua_Integer>(DatetimeHelpers::calendarUnitsBetween(self, other, true)));
        return 1;
    }

    return luaL_error(L, "[Datetime] diffIn unit must be one of millis, seconds, minutes, hours, days, weeks, months, years");
}

int DatetimeModule::methodStartOf(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const std::string_view unit = luaL_checkstring(L, 2);

    TimePoint start;
    TimePoint next;
    if (!DatetimeHelpers::unitBounds(unit, millis, start, next))
    {
        return luaL_error(L, "[Datetime] startOf unit must be one of year, month, week, day, hour, minute, second");
    }

    DatetimeHelpers::push(L, start.time_since_epoch().count());
    return 1;
}

int DatetimeModule::methodEndOf(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const std::string_view unit = luaL_checkstring(L, 2);

    TimePoint start;
    TimePoint next;
    if (!DatetimeHelpers::unitBounds(unit, millis, start, next))
    {
        return luaL_error(L, "[Datetime] endOf unit must be one of year, month, week, day, hour, minute, second");
    }

    DatetimeHelpers::push(L, (next - Millis{1}).time_since_epoch().count());
    return 1;
}

int DatetimeModule::metaToString(lua_State* L)
{
    const std::string out = DatetimeHelpers::formatIso(DatetimeHelpers::check(L, 1), 0);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int DatetimeModule::metaEq(lua_State* L)
{
    lua_pushboolean(L, DatetimeHelpers::check(L, 1) == DatetimeHelpers::check(L, 2));
    return 1;
}

int DatetimeModule::metaLt(lua_State* L)
{
    lua_pushboolean(L, DatetimeHelpers::check(L, 1) < DatetimeHelpers::check(L, 2));
    return 1;
}

int DatetimeModule::metaLe(lua_State* L)
{
    lua_pushboolean(L, DatetimeHelpers::check(L, 1) <= DatetimeHelpers::check(L, 2));
    return 1;
}

int DatetimeModule::luaOpen(lua_State* L)
{
    if (luaL_newmetatable(L, kDatetimeMeta))
    {
        lua_pushcfunction(L, &DatetimeModule::metaToString);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, &DatetimeModule::metaEq);
        lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, &DatetimeModule::metaLt);
        lua_setfield(L, -2, "__lt");
        lua_pushcfunction(L, &DatetimeModule::metaLe);
        lua_setfield(L, -2, "__le");

        lua_newtable(L);

        const luaL_Reg methods[] = {
            {"unix", &DatetimeModule::methodUnix},
            {"millis", &DatetimeModule::methodMillis},
            {"iso", &DatetimeModule::methodIso},
            {"format", &DatetimeModule::methodFormat},
            {"fields", &DatetimeModule::methodFields},
            {"year", &DatetimeModule::methodYear},
            {"month", &DatetimeModule::methodMonth},
            {"day", &DatetimeModule::methodDay},
            {"hour", &DatetimeModule::methodHour},
            {"minute", &DatetimeModule::methodMinute},
            {"second", &DatetimeModule::methodSecond},
            {"weekday", &DatetimeModule::methodWeekday},
            {"yearday", &DatetimeModule::methodYearday},
            {"weekdayName", &DatetimeModule::methodWeekdayName},
            {"monthName", &DatetimeModule::methodMonthName},
            {"isLeapYear", &DatetimeModule::methodIsLeapYear},
            {"daysInMonth", &DatetimeModule::methodDaysInMonth},
            {"add", &DatetimeModule::methodAdd},
            {"subtract", &DatetimeModule::methodSubtract},
            {"diff", &DatetimeModule::methodDiff},
            {"diffIn", &DatetimeModule::methodDiffIn},
            {"startOf", &DatetimeModule::methodStartOf},
            {"endOf", &DatetimeModule::methodEndOf},
            {nullptr, nullptr},
        };
        luaL_setfuncs(L, methods, 0);

        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);

    const luaL_Reg functions[] = {
        {"now", &DatetimeModule::luaNow},
        {"fromUnix", &DatetimeModule::luaFromUnix},
        {"fromMillis", &DatetimeModule::luaFromMillis},
        {"parse", &DatetimeModule::luaParse},
        {"fromFields", &DatetimeModule::luaFromFields},
        {"min", &DatetimeModule::luaMin},
        {"max", &DatetimeModule::luaMax},
        {nullptr, nullptr},
    };
    luaL_newlib(L, functions);
    return 1;
}

void DatetimeModule::install(lua_State* L)
{
    luaL_requiref(L, "datetime", &DatetimeModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::datetime
