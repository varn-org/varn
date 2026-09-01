-- datetime scalar weekday/yearday methods and the error paths for fromFields, min/max arity, and invalid units
local datetime = require("datetime")

local d = datetime.parse("2026-06-21T12:00:00Z")
local other = datetime.parse("2026-03-19T00:00:00Z")

-- the scalar weekday() and yearday() methods agree with the fields() table
assert(d:weekday() == d:fields().weekday, "weekday() should match fields().weekday")
assert(d:yearday() == d:fields().yearday, "yearday() should match fields().yearday")

-- fromFields requires the year, month and day and rejects an invalid calendar date
assert(not pcall(function() return datetime.fromFields({ month = 6, day = 21 }) end), "fromFields without a year should error")
assert(not pcall(function() return datetime.fromFields({ year = 2026, month = 13, day = 1 }) end), "fromFields with month 13 should error")

-- min and max need at least one datetime
assert(not pcall(function() return datetime.min() end), "min with no arguments should error")
assert(not pcall(function() return datetime.max() end), "max with no arguments should error")

-- an unknown unit is rejected on diffIn, startOf and endOf
assert(not pcall(function() return d:diffIn(other, "fortnights") end), "an invalid diffIn unit should error")
assert(not pcall(function() return d:startOf("fortnight") end), "an invalid startOf unit should error")
assert(not pcall(function() return d:endOf("fortnight") end), "an invalid endOf unit should error")

print("datetime errors ok")
