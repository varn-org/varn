-- builds the same instant from unix seconds, millis, and a fields table
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

-- a fields table read at a fixed offset normalizes the wall clock to utc
local atOffset = datetime.fromFields({ year = 2025, month = 6, day = 21, hour = 14, minute = 30, second = 45, offset = 120 })
print("fromFields +02:00:", atOffset:iso())
assert(atOffset == fromUnix, "offset fields land on the same utc instant")

-- now() yields a live instant past any fixed point in the past
assert(datetime.now() > datetime.fromUnix(0), "now is after the epoch")

print("datetime constructors ok")
