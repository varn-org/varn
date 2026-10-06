local datetime = require("datetime")

-- Parse and ISO round-trip in UTC.
assert(datetime.parse("2026-06-21T12:30:00Z"):iso() == "2026-06-21T12:30:00Z", "ISO round-trip")
assert(datetime.parse("2026-06-21"):iso() == "2026-06-21T00:00:00Z", "A bare date is midnight UTC")
assert(datetime.parse("2026-06-21 12:30:00"):iso() == "2026-06-21T12:30:00Z", "Space separator")

-- A trailing offset is normalized back to UTC.
assert(datetime.parse("2026-06-21T12:30:00+03:00"):iso() == "2026-06-21T09:30:00Z", "Positive offset to UTC")
assert(datetime.parse("2026-06-21T12:30:00-0500"):iso() == "2026-06-21T17:30:00Z", "Negative offset to UTC")

-- Sub-second precision survives `parse`, `iso`, and `millis`.
local frac = datetime.parse("2026-06-21T12:30:00.123Z")
assert(frac:iso() == "2026-06-21T12:30:00.123Z", "Fractional ISO")
assert(frac:millis() % 1000 == 123, "Fractional millis")

-- The `fromFields` call builds the same instant as the equivalent ISO string.
assert(datetime.fromFields({ year = 2026, month = 6, day = 21, hour = 12, minute = 30 })
    == datetime.parse("2026-06-21T12:30:00Z"), 'The call "fromFields" equals "parse"')

-- Both `fromUnix` and `fromMillis` build instants from a timestamp and round-trip.
assert(datetime.fromUnix(1750509045):iso() == "2025-06-21T12:30:45Z", 'The call "fromUnix"')
assert(datetime.fromUnix(1750509045):unix() == 1750509045, "Unix round-trip")
assert(datetime.fromMillis(1750509045123):iso() == "2025-06-21T12:30:45.123Z", 'The call "fromMillis" keeps millis')
assert(datetime.fromMillis(1750509045123):millis() == 1750509045123, "Millis round-trip")
assert(datetime.fromUnix(1750509045) == datetime.fromMillis(1750509045000), 'The call "fromUnix" equals "fromMillis"')

-- The `fromFields` call reads the wall clock at a fixed offset and normalizes to UTC.
assert(datetime.fromFields({ year = 2026, month = 6, day = 21, hour = 14, minute = 30, offset = 120 })
    == datetime.parse("2026-06-21T12:30:00Z"), 'The call "fromFields" with an offset')

-- Field access returns the calendar and clock fields.
local f = datetime.parse("2026-06-21T12:30:45Z"):fields()
assert(f.year == 2026 and f.month == 6 and f.day == 21, "Date fields")
assert(f.hour == 12 and f.minute == 30 and f.second == 45, "Time fields")
assert(f.weekday == 7 and f.yearday == 172, "2026-06-21 is a Sunday, day 172")

local d = datetime.parse("2026-06-21T12:30:45Z")
assert(d:year() == 2026 and d:month() == 6 and d:day() == 21, "Scalar date accessors")
assert(d:hour() == 12 and d:minute() == 30 and d:second() == 45, "Scalar time accessors")
assert(d:weekdayName() == "Sunday" and d:monthName() == "June", "Named accessors")

-- Leap years and month lengths are calendar-correct.
assert(datetime.parse("2024-02-10"):isLeapYear() and datetime.parse("2024-02-10"):daysInMonth() == 29, "Leap February")
assert(not datetime.parse("2026-02-10"):isLeapYear() and datetime.parse("2026-02-10"):daysInMonth() == 28, "Common February")
assert(datetime.parse("2026-06-10"):daysInMonth() == 30, "June has 30 days")

-- Calendar-aware arithmetic clamps to the end of a shorter month.
assert(datetime.parse("2026-01-31T00:00:00Z"):add({ months = 1 }):iso() == "2026-02-28T00:00:00Z", "January 31 + 1 month clamps")
assert(datetime.parse("2024-02-29T00:00:00Z"):add({ years = 1 }):iso() == "2025-02-28T00:00:00Z", "Leap day + 1 year clamps")

-- Duration arithmetic crosses day boundaries.
assert(datetime.parse("2026-06-21T12:00:00Z"):add({ hours = 25 }):iso() == "2026-06-22T13:00:00Z", "Add 25 hours")
local base = datetime.parse("2026-06-21T12:00:00Z")
assert(base:add({ days = 3, hours = 5 }):subtract({ days = 3, hours = 5 }) == base, "Add then subtract is identity")

-- The `strftime`-style formatting works in UTC and at a fixed offset.
local fmt = datetime.parse("2026-06-21T08:05:09Z")
assert(fmt:format("%Y-%m-%d %H:%M:%S") == "2026-06-21 08:05:09", "Format pads fields")
assert(fmt:format("%A %B %j") == "Sunday June 172", "Format names and yearday")
assert(fmt:format("%H:%M", 90) == "09:35", "Format shifts by the offset")

-- Differences work in plain and calendar units.
local later = datetime.parse("2026-03-15")
local earlier = datetime.parse("2026-01-10")
assert(later:diffIn(earlier, "days") == 64, "64 days apart")
assert(later:diffIn(earlier, "months") == 2, "2 whole months apart")
assert(earlier:diffIn(later, "months") == -2, "Diff is signed")
assert(datetime.parse("2026-06-21"):diffIn(datetime.parse("2020-06-21"), "years") == 6, "6 years apart")
assert(datetime.parse("2026-06-21T01:00:00Z"):diff(datetime.parse("2026-06-21T00:00:00Z")) == 3600, "Diff in seconds")

-- The remaining `diffIn` units are exact and truncate toward zero.
local hi = datetime.parse("2026-06-21T12:34:56.789Z")
local lo = datetime.parse("2026-06-20T10:00:00Z")
assert(hi:diffIn(lo, "millis") == 95696789, "Diff in millis")
assert(hi:diffIn(lo, "seconds") == 95696, "Diff in the seconds unit")
assert(hi:diffIn(lo, "minutes") == 1594, "Diff in minutes truncates")
assert(hi:diffIn(lo, "hours") == 26, "Diff in hours truncates")
assert(datetime.parse("2026-07-05"):diffIn(datetime.parse("2026-06-21"), "weeks") == 2, "Diff in weeks")

-- The start and end of a unit are exact.
local m = datetime.parse("2026-06-21T12:30:45.500Z")
assert(m:startOf("day"):iso() == "2026-06-21T00:00:00Z", "Start of day")
assert(m:endOf("day"):iso() == "2026-06-21T23:59:59.999Z", "End of day")
assert(m:startOf("month"):iso() == "2026-06-01T00:00:00Z", "Start of month")
assert(m:endOf("month"):iso() == "2026-06-30T23:59:59.999Z", "End of month")
assert(m:startOf("year"):iso() == "2026-01-01T00:00:00Z", "Start of year")
assert(m:endOf("year"):iso() == "2026-12-31T23:59:59.999Z", "End of year")
assert(m:startOf("week"):iso() == "2026-06-15T00:00:00Z", "ISO week starts on Monday")
assert(m:endOf("week"):iso() == "2026-06-21T23:59:59.999Z", "ISO week ends on Sunday")
assert(m:startOf("hour"):iso() == "2026-06-21T12:00:00Z", "Start of hour")
assert(m:endOf("hour"):iso() == "2026-06-21T12:59:59.999Z", "End of hour")
assert(m:startOf("minute"):iso() == "2026-06-21T12:30:00Z", "Start of minute")
assert(m:endOf("minute"):iso() == "2026-06-21T12:30:59.999Z", "End of minute")
assert(m:startOf("second"):iso() == "2026-06-21T12:30:45Z", "Start of second drops millis")
assert(m:endOf("second"):iso() == "2026-06-21T12:30:45.999Z", "End of second")

-- Fixed-offset rendering keeps the same instant but shifts the wall clock.
local noon = datetime.parse("2026-06-21T12:00:00Z")
assert(noon:iso(180) == "2026-06-21T15:00:00+03:00", "Render at +03:00")
assert(noon:iso(-300) == "2026-06-21T07:00:00-05:00", "Render at -05:00")
local of = noon:fields(180)
assert(of.hour == 15 and of.offset == 180, "Fields at an offset")

-- Comparisons and the `min` and `max` calls order instants.
local a = datetime.parse("2026-01-01")
local b = datetime.parse("2026-02-01")
assert(a < b and a <= b and b > a and a ~= b, "Ordering operators")
assert(datetime.min(b, a, datetime.parse("2026-03-01")) == a, 'The call "min"')
assert(datetime.max(a, b, datetime.parse("2025-01-01")) == b, 'The call "max"')

-- The `tostring` call yields ISO text and `now()` is sane.
assert(tostring(datetime.parse("2026-06-21T12:30:00Z")) == "2026-06-21T12:30:00Z", 'The call "tostring" is ISO')
assert(datetime.now():millis() > 1577836800000, 'The call "now" is after 2020')

-- A string with no separators or junk is rejected.
assert(not pcall(datetime.parse, "not a date"), "Rejects garbage")
assert(not pcall(datetime.parse, "2026-13-01"), "Rejects an invalid month")

print("The \"datetime\" tests passed.")
