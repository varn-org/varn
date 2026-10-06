#include "varn/datetime/DatetimeModule.h"

#include <date/date.h>
#include <lua.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace varn::datetime
{

namespace
{
constexpr char kDatetimeMeta[] = "varn.datetime";

// Every unit holds 64 bits, since a standard library may count minutes and hours in 32 bits, which the minutes of the supported years overflow.
using Millis = std::chrono::duration<int64_t, std::milli>;
using Minutes = std::chrono::duration<int64_t, std::ratio<60>>;
using Hours = std::chrono::duration<int64_t, std::ratio<3600>>;
using TimePoint = date::sys_time<Millis>;

class DatetimeHelpers
{
public:
    // The supported instants are those of the years ISO 8601 writes with four digits, which every calendar step of the module handles without overflow.
    static constexpr int kMinYear = 0;
    static constexpr int kMaxYear = 9999;
    static constexpr int64_t kMinMillis = std::chrono::duration_cast<Millis>(date::sys_days{date::year{kMinYear} / date::January / 1}.time_since_epoch()).count();
    static constexpr int64_t kMaxMillis = std::chrono::duration_cast<Millis>(date::sys_days{date::year{kMaxYear + 1} / date::January / 1}.time_since_epoch()).count() - 1;
    static constexpr int64_t kSpanMillis = kMaxMillis - kMinMillis;

    // A fixed offset lies within a day on either side of UTC.
    static constexpr lua_Integer kMaxOffsetMinutes = 24 * 60 - 1;

    static int64_t check(lua_State* L, int index)
    {
        return *static_cast<int64_t*>(luaL_checkudata(L, index, kDatetimeMeta));
    }

    // Every instant the module answers goes through here, so none of them lies outside the supported years.
    static void push(lua_State* L, int64_t millis)
    {
        if (millis < kMinMillis || millis > kMaxMillis)
        {
            luaL_error(L, "The instant lies outside the supported years 0000 to 9999.");
        }

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

    // Answers the instant of a day of the calendar at a time of day, written with an offset from UTC, in milliseconds since the epoch.
    static int64_t civilMillis(const date::year_month_day& ymd, int64_t hour, int64_t minute, int64_t second, int64_t millis, int64_t offsetMinutes)
    {
        const TimePoint local = TimePoint{date::sys_days{ymd}} + Hours{hour} + Minutes{minute} + std::chrono::duration<int64_t>{second} + Millis{millis};
        return (local - Minutes{offsetMinutes}).time_since_epoch().count();
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
        const TimePoint shifted = DatetimeHelpers::toTimePoint(millis) + Minutes{offsetMinutes};
        const std::string suffix = DatetimeHelpers::offsetSuffix(offsetMinutes);

        if (millis % 1000 == 0)
        {
            return date::format("%Y-%m-%dT%H:%M:%S", std::chrono::floor<std::chrono::seconds>(shifted)) + suffix;
        }

        return date::format("%Y-%m-%dT%H:%M:%S", shifted) + suffix;
    }

    static void pushFields(lua_State* L, int64_t millis, int offsetMinutes)
    {
        const TimePoint shifted = DatetimeHelpers::toTimePoint(millis) + Minutes{offsetMinutes};
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

    // Reads the offset in minutes at `index`, which defaults to UTC.
    static int checkOffset(lua_State* L, int index)
    {
        const lua_Integer offset = luaL_optinteger(L, index, 0);
        if (offset < -kMaxOffsetMinutes || offset > kMaxOffsetMinutes)
        {
            luaL_error(L, "An offset must lie from %I to %I minutes, not %I.", -kMaxOffsetMinutes, kMaxOffsetMinutes, offset);
        }

        return static_cast<int>(offset);
    }

    static int64_t shiftInstant(int64_t millis, int years, int months, long long days, long long hours, long long minutes, long long seconds, long long millisDelta)
    {
        const TimePoint tp = DatetimeHelpers::toTimePoint(millis);
        const date::sys_days dp = std::chrono::floor<date::days>(tp);
        const Millis timeOfDay = tp - dp;

        const date::year_month_day ymd = date::year_month_day{dp} + date::years{years} + date::months{months};

        // Snaps to the last day when the shifted month is shorter.
        const date::sys_days shiftedDay = ymd.ok() ? date::sys_days{ymd} : date::sys_days{ymd.year() / ymd.month() / date::last};

        const TimePoint result = shiftedDay + timeOfDay + date::days{static_cast<int>(days)} + Hours{hours} + Minutes{minutes} + std::chrono::seconds{seconds} + Millis{millisDelta};

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
            start = std::chrono::floor<Hours>(tp);
            next = std::chrono::floor<Hours>(tp) + Hours{1};
            return true;
        }
        if (unit == "minute")
        {
            start = std::chrono::floor<Minutes>(tp);
            next = std::chrono::floor<Minutes>(tp) + Minutes{1};
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

            if (hour > 23 || minute > 59 || second > 59)
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

                if (offHour > 23 || offMin > 59)
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

        return DatetimeHelpers::civilMillis(ymd, hour, minute, second, millisOfSecond, offsetMinutes);
    }

    // Reads an integer field of a table, or nothing when it is absent, naming the key when it holds anything else.
    static std::optional<lua_Integer> optionalField(lua_State* L, int table, const char* key)
    {
        lua_getfield(L, table, key);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return std::nullopt;
        }

        int isInteger = 0;
        const lua_Integer value = lua_tointegerx(L, -1, &isInteger);
        if (isInteger == 0)
        {
            luaL_error(L, "The field \"%s\" must be an integer.", key);
        }

        lua_pop(L, 1);
        return value;
    }

    // Reads a field of a wall-clock time, refusing a value outside its range on the full width it was given.
    static lua_Integer boundedField(lua_State* L, int table, const char* key, lua_Integer low, lua_Integer high, std::optional<lua_Integer> fallback)
    {
        const std::optional<lua_Integer> read = DatetimeHelpers::optionalField(L, table, key);
        if (!read && !fallback)
        {
            luaL_error(L, "The call \"fromFields\" requires the \"%s\" field.", key);
        }

        const lua_Integer value = read ? *read : *fallback;
        if (value < low || value > high)
        {
            luaL_error(L, "The field \"%s\" must lie from %I to %I, not %I.", key, low, high, value);
        }

        return value;
    }

    // Reads one delta of a shift, which may not be larger than the span of the supported years in its own unit, since a larger one always leaves them.
    static long long deltaField(lua_State* L, int table, const char* key, int64_t unitMillis)
    {
        const lua_Integer value = DatetimeHelpers::optionalField(L, table, key).value_or(0);
        const lua_Integer bound = kSpanMillis / unitMillis;
        if (value < -bound || value > bound)
        {
            luaL_error(L, "The delta \"%s\" moves the instant outside the supported years 0000 to 9999.", key);
        }

        return static_cast<long long>(value);
    }

    // Refuses a key of a shift that names no unit, so a misspelled unit never shifts by nothing.
    static void checkDeltaKeys(lua_State* L, int table)
    {
        static constexpr std::array<std::string_view, 8> kUnits{"years", "months", "weeks", "days", "hours", "minutes", "seconds", "millis"};

        lua_pushnil(L);
        while (lua_next(L, table) != 0)
        {
            lua_pop(L, 1);
            std::size_t length = 0;
            const char* name = lua_type(L, -1) == LUA_TSTRING ? lua_tolstring(L, -1, &length) : nullptr;
            if (name == nullptr || std::find(kUnits.begin(), kUnits.end(), std::string_view(name, length)) == kUnits.end())
            {
                luaL_error(L, "The delta \"%s\" names no unit. Use \"years\", \"months\", \"weeks\", \"days\", \"hours\", \"minutes\", \"seconds\" or \"millis\".", luaL_tolstring(L, -1, nullptr));
            }
        }
    }

    static int64_t shiftByDeltaTable(lua_State* L, int64_t millis, int table, int sign)
    {
        checkDeltaKeys(L, table);

        // A calendar unit is bounded by its shortest length, so no delta that could still land inside the supported years is refused.
        constexpr int64_t kDayMillis = 86400000;
        constexpr int64_t kYearMillis = 365 * kDayMillis;
        constexpr int64_t kMonthMillis = 28 * kDayMillis;

        const long long years = DatetimeHelpers::deltaField(L, table, "years", kYearMillis);
        const long long months = DatetimeHelpers::deltaField(L, table, "months", kMonthMillis);
        const long long weeks = DatetimeHelpers::deltaField(L, table, "weeks", 7 * kDayMillis);
        const long long days = DatetimeHelpers::deltaField(L, table, "days", kDayMillis);
        const long long hours = DatetimeHelpers::deltaField(L, table, "hours", 3600000);
        const long long minutes = DatetimeHelpers::deltaField(L, table, "minutes", 60000);
        const long long seconds = DatetimeHelpers::deltaField(L, table, "seconds", 1000);
        const long long millisDelta = DatetimeHelpers::deltaField(L, table, "millis", 1);

        // Each delta is bounded by the span of the supported years, so every sum below stays far inside the calendar types and the instant, and the result is checked when it is pushed.
        return DatetimeHelpers::shiftInstant(millis, static_cast<int>(sign * years), static_cast<int>(sign * months), sign * (weeks * 7 + days), sign * hours, sign * minutes, sign * seconds, sign * millisDelta);
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
    const lua_Number millis = luaL_checknumber(L, 1) * 1000.0;

    // The comparison is written so that a value that is not a number fails it too.
    if (!(millis >= static_cast<lua_Number>(DatetimeHelpers::kMinMillis) && millis <= static_cast<lua_Number>(DatetimeHelpers::kMaxMillis)))
    {
        return luaL_error(L, "The instant lies outside the supported years 0000 to 9999.");
    }

    DatetimeHelpers::push(L, static_cast<int64_t>(std::llround(millis)));
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
        return luaL_error(L, "The text \"%s\" is not an ISO 8601 date and time.", data);
    }

    DatetimeHelpers::push(L, *millis);
    return 1;
}

int DatetimeModule::luaFromFields(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    const lua_Integer year = DatetimeHelpers::boundedField(L, 1, "year", DatetimeHelpers::kMinYear, DatetimeHelpers::kMaxYear, std::nullopt);
    const lua_Integer month = DatetimeHelpers::boundedField(L, 1, "month", 1, 12, std::nullopt);
    const lua_Integer day = DatetimeHelpers::boundedField(L, 1, "day", 1, 31, std::nullopt);
    const lua_Integer hour = DatetimeHelpers::boundedField(L, 1, "hour", 0, 23, 0);
    const lua_Integer minute = DatetimeHelpers::boundedField(L, 1, "minute", 0, 59, 0);
    const lua_Integer second = DatetimeHelpers::boundedField(L, 1, "second", 0, 59, 0);
    const lua_Integer millisOfSecond = DatetimeHelpers::boundedField(L, 1, "millis", 0, 999, 0);
    const lua_Integer offsetMinutes = DatetimeHelpers::boundedField(L, 1, "offset", -DatetimeHelpers::kMaxOffsetMinutes, DatetimeHelpers::kMaxOffsetMinutes, 0);

    const date::year_month_day ymd{date::year{static_cast<int>(year)}, date::month{static_cast<unsigned>(month)}, date::day{static_cast<unsigned>(day)}};
    if (!ymd.ok())
    {
        return luaL_error(L, "The fields \"year\", \"month\" and \"day\" name no day of the calendar.");
    }

    DatetimeHelpers::push(L, DatetimeHelpers::civilMillis(ymd, hour, minute, second, millisOfSecond, offsetMinutes));
    return 1;
}

int DatetimeModule::luaMin(lua_State* L)
{
    const int count = lua_gettop(L);
    if (count < 1)
    {
        return luaL_error(L, "The call \"min\" requires at least one datetime.");
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
        return luaL_error(L, "The call \"max\" requires at least one datetime.");
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
    const int offsetMinutes = DatetimeHelpers::checkOffset(L, 2);

    const std::string out = DatetimeHelpers::formatIso(millis, offsetMinutes);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int DatetimeModule::methodFormat(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const char* fmt = luaL_optstring(L, 2, "%Y-%m-%dT%H:%M:%SZ");
    const int offsetMinutes = DatetimeHelpers::checkOffset(L, 3);

    const TimePoint shifted = DatetimeHelpers::toTimePoint(millis) + Minutes{offsetMinutes};
    const std::string out = date::format(fmt, std::chrono::floor<std::chrono::seconds>(shifted));
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int DatetimeModule::methodFields(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const int offsetMinutes = DatetimeHelpers::checkOffset(L, 2);
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

    return luaL_error(L, "The \"diffIn\" unit must be one of \"millis\", \"seconds\", \"minutes\", \"hours\", \"days\", \"weeks\", \"months\", \"years\".");
}

int DatetimeModule::methodStartOf(lua_State* L)
{
    const int64_t millis = DatetimeHelpers::check(L, 1);
    const std::string_view unit = luaL_checkstring(L, 2);

    TimePoint start;
    TimePoint next;
    if (!DatetimeHelpers::unitBounds(unit, millis, start, next))
    {
        return luaL_error(L, "The \"startOf\" unit must be one of \"year\", \"month\", \"week\", \"day\", \"hour\", \"minute\", \"second\".");
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
        return luaL_error(L, "The \"endOf\" unit must be one of \"year\", \"month\", \"week\", \"day\", \"hour\", \"minute\", \"second\".");
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
