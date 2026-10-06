-- Covers the scalar `weekday` and `yearday` methods and the error paths for `fromFields`, the `min` and `max` arity, and invalid units.
local datetime = require("datetime")

local d = datetime.parse("2026-06-21T12:00:00Z")
local other = datetime.parse("2026-03-19T00:00:00Z")

-- The scalar `weekday()` and `yearday()` methods agree with the `fields()` table.
assert(d:weekday() == d:fields().weekday, 'The call "weekday()" should match "fields().weekday"')
assert(d:yearday() == d:fields().yearday, 'The call "yearday()" should match "fields().yearday"')

-- The `fromFields` call requires the year, month and day and rejects an invalid calendar date.
assert(not pcall(function() return datetime.fromFields({ month = 6, day = 21 }) end), 'The call "fromFields" without a year should error')
assert(not pcall(function() return datetime.fromFields({ year = 2026, month = 13, day = 1 }) end), 'The call "fromFields" with month 13 should error')

-- The `min` and `max` calls need at least one datetime.
assert(not pcall(function() return datetime.min() end), 'The call "min" with no arguments should error')
assert(not pcall(function() return datetime.max() end), 'The call "max" with no arguments should error')

-- An unknown unit is rejected on `diffIn`, `startOf` and `endOf`.
assert(not pcall(function() return d:diffIn(other, "fortnights") end), 'An invalid "diffIn" unit should error')
assert(not pcall(function() return d:startOf("fortnight") end), 'An invalid "startOf" unit should error')
assert(not pcall(function() return d:endOf("fortnight") end), 'An invalid "endOf" unit should error')

print("The \"datetime\" error tests passed.")
