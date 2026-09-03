# 🕒 datetime

Parsing, formatting, and calendar arithmetic on instants. An instant is a single point in time held with millisecond precision, internally as UTC, so two instants always compare and subtract cleanly no matter how they were built. Backed by the date.h calendar library, available in every build including the browser.

## Constructors

- `datetime.now()` → an instant for the current moment.
- `datetime.fromUnix(seconds)` → an instant at a Unix timestamp. Fractional seconds are kept.
- `datetime.fromMillis(ms)` → an instant at a Unix timestamp in milliseconds.
- `datetime.parse(text)` → an instant from an ISO-8601 string. Accepts a bare date (`"2026-06-21"`), a date-time with a `T` or space separator, optional fractional seconds, and an optional trailing `Z` or `+hh:mm` / `-hh:mm` offset. A string with no offset is read as UTC. Raises on anything it cannot parse.
- `datetime.fromFields(t)` → an instant from a table of fields. `year`, `month`, and `day` are required. `hour`, `minute`, `second`, `millis`, and `offset` (in minutes) default to `0`. The fields are read as the wall-clock time at `offset`, then normalized to UTC.
- `datetime.min(...)` / `datetime.max(...)` → the earliest or latest of the given instants.

## Reading an instant

- `dt:unix()` → the Unix timestamp in whole seconds.
- `dt:millis()` → the Unix timestamp in milliseconds.
- `dt:fields(offset?)` → a table with `year`, `month`, `day`, `hour`, `minute`, `second`, `millis`, `weekday` (1=Monday … 7=Sunday), `yearday` (1–366), and `offset`. With an `offset` in minutes the fields are given for that fixed offset instead of UTC.
- `dt:year()`, `dt:month()`, `dt:day()`, `dt:hour()`, `dt:minute()`, `dt:second()` → the individual UTC fields as numbers.
- `dt:weekday()` → the ISO weekday (1=Monday … 7=Sunday). `dt:yearday()` → the day of the year.
- `dt:weekdayName()` → `"Monday"` … `"Sunday"`. `dt:monthName()` → `"January"` … `"December"`.
- `dt:isLeapYear()` → whether the instant's year is a leap year. `dt:daysInMonth()` → days in its month.

## Formatting

- `dt:iso(offset?)` → an ISO-8601 string. Without an argument it is UTC with a trailing `Z`. With an `offset` in minutes it renders the wall clock at that offset and ends with `+hh:mm` / `-hh:mm`. Milliseconds are included only when non-zero.
- `dt:format(fmt?, offset?)` → a string rendered with a strftime-style format (`%Y`, `%m`, `%d`, `%H`, `%M`, `%S`, `%A`, `%j`, …), in UTC or at the given fixed `offset`. Defaults to the ISO form.
- `tostring(dt)` → the same as `dt:iso()`.

## Arithmetic

- `dt:add(delta)` / `dt:subtract(delta)` → a new instant shifted by the fields of `delta`. The receiver is unchanged. `delta` may set any of `years`, `months`, `weeks`, `days`, `hours`, `minutes`, `seconds`, `millis`. `years` and `months` are **calendar-aware**: adding a month to January 31 lands on the last day of February, not on an invalid date.
- `dt:diff(other)` → whole seconds from `other` to `dt` (`dt - other`), positive when `dt` is later.
- `dt:diffIn(other, unit)` → the signed difference in `unit`, one of `"millis"`, `"seconds"`, `"minutes"`, `"hours"`, `"days"`, `"weeks"`, `"months"`, `"years"`. `months` and `years` count whole calendar units. The rest are exact and truncate toward zero.
- `dt:startOf(unit)` / `dt:endOf(unit)` → the first or last instant of the surrounding `unit`, one of `"year"`, `"month"`, `"week"` (ISO, starting Monday), `"day"`, `"hour"`, `"minute"`, `"second"`.

## Comparison

Instants compare with the standard operators (`==`, `<`, `<=`, `>`, `>=`), so they sort and can be used as table keys for ordering. Two instants are equal when they point at the same millisecond.

## Examples

### `arithmetic.lua`

```lua
-- Shifts instants by mixed units and measures gaps in several units.
local datetime = require("datetime")

local base = datetime.parse("2026-06-21T12:00:00Z")

-- Add and subtract accept any mix of calendar and clock units.
print("plus 2 weeks 3 days:", base:add({ weeks = 2, days = 3 }):iso())
print("minus 90 minutes:   ", base:subtract({ minutes = 90 }):iso())
assert(base:add({ weeks = 1 }) == base:add({ days = 7 }), "a week is seven days")

-- Diff is whole seconds, diffIn reports any unit signed and truncating.
local earlier = datetime.parse("2026-06-20T10:00:00Z")
print("seconds apart:", base:diff(earlier))
print("hours apart:  ", base:diffIn(earlier, "hours"))
print("minutes apart:", base:diffIn(earlier, "minutes"))
print("weeks apart:  ", datetime.parse("2026-07-05T12:00:00Z"):diffIn(base, "weeks"))
assert(base:diffIn(earlier, "hours") == 26, "26 hours apart")

-- Both startOf and endOf snap to sub-day boundaries.
local t = datetime.parse("2026-06-21T12:34:56.789Z")
print("start of hour:  ", t:startOf("hour"):iso())
print("end of minute:  ", t:endOf("minute"):iso())
print("start of week:  ", t:startOf("week"):iso())
print("end of year:    ", t:endOf("year"):iso())
assert(t:startOf("second"):iso() == "2026-06-21T12:34:56Z", "start of second drops millis")

print("datetime arithmetic ok")
```

### `basic.lua`

```lua
-- Parsing, calendar arithmetic, diffs, and fixed-offset rendering.
local datetime = require("datetime")

-- Parses an iso-8601 string with an offset and normalizes to utc.
local launch = datetime.parse("2026-06-21T09:30:00-03:00")
print("launch (utc):", launch:iso())
print("weekday:", launch:weekdayName(), "day of year:", launch:fields().yearday)

-- Calendar arithmetic understands months and the end of a short month.
local trialEnd = datetime.parse("2026-01-31"):add({ months = 1 })
print("jan 31 + 1 month:", trialEnd:iso())

-- Diffs come in plain units or calendar units.
local a = datetime.parse("2026-03-15")
local b = datetime.parse("2026-01-10")
print("days apart:", a:diffIn(b, "days"))
print("months apart:", a:diffIn(b, "months"))

-- Snaps to the boundaries of a unit.
local now = datetime.parse("2026-06-21T12:30:45Z")
print("start of month:", now:startOf("month"):iso())
print("end of day:", now:endOf("day"):iso())

-- Renders the same instant against a fixed utc offset.
print("in +05:30:", now:iso(330))

-- Instants compare with the usual operators.
assert(b < a)
print("earliest:", datetime.min(a, b):iso())
```

### `constructors.lua`

```lua
-- Builds the same instant from unix seconds, millis, and a fields table.
local datetime = require("datetime")

local seconds = 1750509045
local fromUnix = datetime.fromUnix(seconds)
print("fromUnix:  ", fromUnix:iso())
assert(fromUnix:unix() == seconds, "unix round-trip")

local fromMillis = datetime.fromMillis(seconds * 1000 + 123)
print("fromMillis:", fromMillis:iso())
assert(fromMillis:millis() == seconds * 1000 + 123, "millis round-trip")

local fromFields = datetime.fromFields({ year = 2025, month = 6, day = 21, hour = 12, minute = 30, second = 45 })
print("fromFields:", fromFields:iso())
assert(fromFields:unix() == seconds, "fromFields matches the same instant")

-- A fields table read at a fixed offset normalizes the wall clock to utc.
local atOffset = datetime.fromFields({ year = 2025, month = 6, day = 21, hour = 14, minute = 30, second = 45, offset = 120 })
print("fromFields +02:00:", atOffset:iso())
assert(atOffset == fromUnix, "offset fields land on the same utc instant")

-- A call to now() yields a live instant past any fixed point in the past.
assert(datetime.now() > datetime.fromUnix(0), "now is after the epoch")

print("datetime constructors ok")
```

### `fields.lua`

```lua
-- Reads scalar fields and renders them with a strftime-style format.
local datetime = require("datetime")

local dt = datetime.parse("2026-06-21T08:05:09Z")

-- Individual scalar accessors return numbers.
print("y/m/d:", dt:year(), dt:month(), dt:day())
print("h/m/s:", dt:hour(), dt:minute(), dt:second())
print("weekday:", dt:weekday(), dt:weekdayName())
print("yearday:", dt:yearday())
print("month:", dt:monthName(), "leap year:", dt:isLeapYear(), "days in month:", dt:daysInMonth())

-- Strftime-style formatting in utc and at a fixed offset.
print("formatted:", dt:format("%A %d %B %Y %H:%M:%S"))
print("at +05:30:", dt:format("%Y-%m-%d %H:%M", 330))
assert(dt:format("%Y-%m-%d") == "2026-06-21", "format date fields")
assert(dt:format("%H:%M:%S", 60) == "09:05:09", "format shifts by the offset")

print("datetime fields ok")
```
## Under the hood

Calendar conversion, leap-year handling, ISO formatting, and field decomposition come from Howard Hinnant's date.h. Named IANA time zones (which require shipping the tz database) are intentionally not included. The module covers UTC and fixed offsets, which is what ISO-8601 expresses.
