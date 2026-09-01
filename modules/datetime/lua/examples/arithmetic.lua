-- shifts instants by mixed units and measures gaps in several units
local datetime = require("datetime")

local base = datetime.parse("2026-06-21T12:00:00Z")

-- add and subtract accept any mix of calendar and clock units
print("plus 2 weeks 3 days:", base:add({ weeks = 2, days = 3 }):iso())
print("minus 90 minutes:   ", base:subtract({ minutes = 90 }):iso())
assert(base:add({ weeks = 1 }) == base:add({ days = 7 }), "a week is seven days")

-- diff is whole seconds, diffIn reports any unit signed and truncating
local earlier = datetime.parse("2026-06-20T10:00:00Z")
print("seconds apart:", base:diff(earlier))
print("hours apart:  ", base:diffIn(earlier, "hours"))
print("minutes apart:", base:diffIn(earlier, "minutes"))
print("weeks apart:  ", datetime.parse("2026-07-05T12:00:00Z"):diffIn(base, "weeks"))
assert(base:diffIn(earlier, "hours") == 26, "26 hours apart")

-- startOf and endOf snap to sub-day boundaries
local t = datetime.parse("2026-06-21T12:34:56.789Z")
print("start of hour:  ", t:startOf("hour"):iso())
print("end of minute:  ", t:endOf("minute"):iso())
print("start of week:  ", t:startOf("week"):iso())
print("end of year:    ", t:endOf("year"):iso())
assert(t:startOf("second"):iso() == "2026-06-21T12:34:56Z", "start of second drops millis")

print("datetime arithmetic ok")
