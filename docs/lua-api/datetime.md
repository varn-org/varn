# 🕒 datetime

Parsing, formatting, and calendar arithmetic on instants. An instant is a single point in time held
with millisecond precision, internally as UTC, so two instants always compare and subtract cleanly no
matter how they were built. Backed by the date.h calendar library, available in every build including
the browser.

## Constructors

- `datetime.now()` → an instant for the current moment.
- `datetime.fromUnix(seconds)` → an instant at a Unix timestamp. Fractional seconds are kept.
- `datetime.fromMillis(ms)` → an instant at a Unix timestamp in milliseconds.
- `datetime.parse(text)` → an instant from an ISO-8601 string. Accepts a bare date (`"2026-06-21"`), a
  date-time with a `T` or space separator, optional fractional seconds, and an optional trailing `Z` or
  `+hh:mm` / `-hh:mm` offset. A string with no offset is read as UTC. Raises on anything it cannot parse.
- `datetime.fromFields(t)` → an instant from a table of fields. `year`, `month`, and `day` are required;
  `hour`, `minute`, `second`, `millis`, and `offset` (in minutes) default to `0`. The fields are read as
  the wall-clock time at `offset`, then normalized to UTC.
- `datetime.min(...)` / `datetime.max(...)` → the earliest or latest of the given instants.

## Reading an instant

- `dt:unix()` → the Unix timestamp in whole seconds.
- `dt:millis()` → the Unix timestamp in milliseconds.
- `dt:fields(offset?)` → a table with `year`, `month`, `day`, `hour`, `minute`, `second`, `millis`,
  `weekday` (1=Monday … 7=Sunday), `yearday` (1–366), and `offset`. With an `offset` in minutes the
  fields are given for that fixed offset instead of UTC.
- `dt:year()`, `dt:month()`, `dt:day()`, `dt:hour()`, `dt:minute()`, `dt:second()` → the individual UTC
  fields as numbers.
- `dt:weekday()` → the ISO weekday (1=Monday … 7=Sunday). `dt:yearday()` → the day of the year.
- `dt:weekdayName()` → `"Monday"` … `"Sunday"`. `dt:monthName()` → `"January"` … `"December"`.
- `dt:isLeapYear()` → whether the instant's year is a leap year. `dt:daysInMonth()` → days in its month.

## Formatting

- `dt:iso(offset?)` → an ISO-8601 string. Without an argument it is UTC with a trailing `Z`; with an
  `offset` in minutes it renders the wall clock at that offset and ends with `+hh:mm` / `-hh:mm`.
  Milliseconds are included only when non-zero.
- `dt:format(fmt?, offset?)` → a string rendered with a strftime-style format (`%Y`, `%m`, `%d`, `%H`,
  `%M`, `%S`, `%A`, `%j`, …), in UTC or at the given fixed `offset`. Defaults to the ISO form.
- `tostring(dt)` → the same as `dt:iso()`.

## Arithmetic

- `dt:add(delta)` / `dt:subtract(delta)` → a new instant shifted by the fields of `delta`; the receiver
  is unchanged. `delta` may set any of `years`, `months`, `weeks`, `days`, `hours`, `minutes`, `seconds`,
  `millis`. `years` and `months` are **calendar-aware**: adding a month to January 31 lands on the last
  day of February, not on an invalid date.
- `dt:diff(other)` → whole seconds from `other` to `dt` (`dt - other`), positive when `dt` is later.
- `dt:diffIn(other, unit)` → the signed difference in `unit`, one of `"millis"`, `"seconds"`,
  `"minutes"`, `"hours"`, `"days"`, `"weeks"`, `"months"`, `"years"`. `months` and `years` count whole
  calendar units; the rest are exact and truncate toward zero.
- `dt:startOf(unit)` / `dt:endOf(unit)` → the first or last instant of the surrounding `unit`, one of
  `"year"`, `"month"`, `"week"` (ISO, starting Monday), `"day"`, `"hour"`, `"minute"`, `"second"`.

## Comparison

Instants compare with the standard operators (`==`, `<`, `<=`, `>`, `>=`), so they sort and can be used
as table keys for ordering. Two instants are equal when they point at the same millisecond.

## Examples

### `basic.lua`

```lua
-- parsing, calendar arithmetic, diffs, and fixed-offset rendering.
local datetime = require("datetime")

-- parse an iso-8601 string with an offset; it normalizes to utc.
local launch = datetime.parse("2026-06-21T09:30:00-03:00")
print("launch (utc):", launch:iso())
print("weekday:", launch:weekdayName(), "day of year:", launch:fields().yearday)

-- calendar arithmetic understands months and the end of a short month.
local trialEnd = datetime.parse("2026-01-31"):add({ months = 1 })
print("jan 31 + 1 month:", trialEnd:iso())

-- diffs come in plain units or calendar units.
local a = datetime.parse("2026-03-15")
local b = datetime.parse("2026-01-10")
print("days apart:", a:diffIn(b, "days"))
print("months apart:", a:diffIn(b, "months"))

-- snap to the boundaries of a unit.
local now = datetime.parse("2026-06-21T12:30:45Z")
print("start of month:", now:startOf("month"):iso())
print("end of day:", now:endOf("day"):iso())

-- render the same instant against a fixed utc offset.
print("in +05:30:", now:iso(330))

-- instants compare with the usual operators.
assert(b < a)
print("earliest:", datetime.min(a, b):iso())
```

## Under the hood

Calendar conversion, leap-year handling, ISO formatting, and field decomposition come from Howard
Hinnant's date.h. Named IANA time zones (which require shipping the tz database) are intentionally not
included; the module covers UTC and fixed offsets, which is what ISO-8601 expresses.
