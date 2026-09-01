-- reads scalar fields and renders them with a strftime-style format
local datetime = require("datetime")

local dt = datetime.parse("2026-06-21T08:05:09Z")

-- individual scalar accessors return numbers
print("y/m/d:", dt:year(), dt:month(), dt:day())
print("h/m/s:", dt:hour(), dt:minute(), dt:second())
print("weekday:", dt:weekday(), dt:weekdayName())
print("yearday:", dt:yearday())
print("month:", dt:monthName(), "leap year:", dt:isLeapYear(), "days in month:", dt:daysInMonth())

-- strftime-style formatting in utc and at a fixed offset
print("formatted:", dt:format("%A %d %B %Y %H:%M:%S"))
print("at +05:30:", dt:format("%Y-%m-%d %H:%M", 330))
assert(dt:format("%Y-%m-%d") == "2026-06-21", "format date fields")
assert(dt:format("%H:%M:%S", 60) == "09:05:09", "format shifts by the offset")

print("datetime fields ok")
