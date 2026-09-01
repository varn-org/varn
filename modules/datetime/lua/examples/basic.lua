-- parsing, calendar arithmetic, diffs, and fixed-offset rendering
local datetime = require("datetime")

-- parses an iso-8601 string with an offset and normalizes to utc
local launch = datetime.parse("2026-06-21T09:30:00-03:00")
print("launch (utc):", launch:iso())
print("weekday:", launch:weekdayName(), "day of year:", launch:fields().yearday)

-- calendar arithmetic understands months and the end of a short month
local trialEnd = datetime.parse("2026-01-31"):add({ months = 1 })
print("jan 31 + 1 month:", trialEnd:iso())

-- diffs come in plain units or calendar units
local a = datetime.parse("2026-03-15")
local b = datetime.parse("2026-01-10")
print("days apart:", a:diffIn(b, "days"))
print("months apart:", a:diffIn(b, "months"))

-- snaps to the boundaries of a unit
local now = datetime.parse("2026-06-21T12:30:45Z")
print("start of month:", now:startOf("month"):iso())
print("end of day:", now:endOf("day"):iso())

-- renders the same instant against a fixed utc offset
print("in +05:30:", now:iso(330))

-- instants compare with the usual operators
assert(b < a)
print("earliest:", datetime.min(a, b):iso())
