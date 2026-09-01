-- datetime shift deltas are bounded so an absurd value raises a clean error instead of silently overflowing the internal integer math
local datetime = require("datetime")

local base = datetime.parse("2026-06-21T12:00:00Z")

assert(base:add({ days = 1 }):millis() > base:millis(), "a normal shift should still work")
assert(base:subtract({ hours = 2 }):millis() < base:millis(), "a normal subtract should still work")

-- year, month and day deltas that would overflow the int-based calendar types are rejected
assert(not pcall(function() return base:add({ years = 5000000000 }) end), "an out-of-range year delta should error")
assert(not pcall(function() return base:add({ months = 9000000000 }) end), "an out-of-range month delta should error")
assert(not pcall(function() return base:add({ days = 5000000000 }) end), "an out-of-range day delta should error")

-- hour and second deltas whose millisecond span would overflow the instant are rejected
assert(not pcall(function() return base:add({ hours = 9000000000000 }) end), "an out-of-range hour delta should error")
assert(not pcall(function() return base:add({ seconds = 9000000000000000 }) end), "an out-of-range second delta should error")

print("datetime overflow ok")
