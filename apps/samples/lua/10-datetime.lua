-- an instant carries methods for formatting, fields and calendar arithmetic
local datetime = require("datetime")

local moment = datetime.parse("2026-09-02T10:30:00Z")
print("iso:", moment:format("%Y-%m-%d %H:%M:%S"))
print("year:", moment:year(), "month:", moment:month(), "day:", moment:day())
print("weekday:", moment:weekdayName(), "day of year:", moment:fields().yearday)

local later = moment:add({ days = 30 })
print("in 30 days:", later:format("%Y-%m-%d"))

print("now:", datetime.now():format("%Y-%m-%d %H:%M:%S"))
