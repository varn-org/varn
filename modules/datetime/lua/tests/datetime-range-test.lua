-- Every instant lies in the years 0000 to 9999, and a field, a delta or a timestamp outside its range raises a sentence instead of overflowing.
local datetime = require("datetime")

local function failsWith(fragment, fn, ...)
    local ok, err = pcall(fn, ...)
    assert(not ok, "The call should raise an error containing: " .. fragment)
    assert(tostring(err):find(fragment, 1, true), "The error should contain '" .. fragment .. "', got " .. tostring(err))
end

-- The first and the last supported instants round-trip through ISO text.
local first = datetime.fromFields({ year = 0, month = 1, day = 1 })
local last = datetime.fromFields({ year = 9999, month = 12, day = 31, hour = 23, minute = 59, second = 59, millis = 999 })
assert(first:iso() == "0000-01-01T00:00:00Z", "The first supported instant is midnight of year 0000, got " .. first:iso())
assert(last:iso() == "9999-12-31T23:59:59.999Z", "The last supported instant ends year 9999, got " .. last:iso())
assert(datetime.parse(last:iso()) == last, "The last supported instant parses back")
assert(last:endOf("year") == last, 'The call "endOf" reaches the last supported instant')

-- A shift or a timestamp past either end is refused.
local outside = "The instant lies outside the supported years 0000 to 9999."
failsWith(outside, function() return last:add({ millis = 1 }) end)
failsWith(outside, function() return first:subtract({ millis = 1 }) end)
failsWith(outside, datetime.fromMillis, math.maxinteger)
failsWith(outside, datetime.fromMillis, math.mininteger)
failsWith(outside, datetime.fromUnix, 0 / 0)
failsWith(outside, datetime.fromUnix, math.huge)
failsWith(outside, datetime.fromUnix, -math.huge)

-- A delta whose sum with the instant leaves the supported years is refused even when the delta alone is small.
local now = datetime.parse("2026-06-21T12:00:00Z")
failsWith(outside, function() return now:add({ years = 8000 }) end)
failsWith('The delta "day" names no unit.', function() return now:add({ day = 1 }) end)
failsWith('names no unit.', function() return now:subtract({ 1 }) end)
failsWith(outside, function() return now:add({ days = 3000000 }) end)
failsWith('The delta "seconds"', function() return now:add({ seconds = 100000000000000000 }) end)
failsWith('The delta "years"', function() return now:add({ years = math.mininteger }) end)
failsWith('The field "days" must be an integer.', function() return now:add({ days = 1.5 }) end)

-- A field is checked on its full width, so a huge value never wraps into an accepted one.
failsWith('The field "hour" must lie from 0 to 23', datetime.fromFields, { year = 2026, month = 1, day = 1, hour = math.maxinteger })
failsWith('The field "year" must lie from 0 to 9999', datetime.fromFields, { year = 4294969322, month = 1, day = 1 })
failsWith('The field "minute" must lie from 0 to 59', datetime.fromFields, { year = 2026, month = 1, day = 1, minute = 60 })
failsWith('The field "offset" must lie from -1439 to 1439', datetime.fromFields, { year = 2026, month = 1, day = 1, offset = 1440 })
failsWith('The field "year" must be an integer.', datetime.fromFields, { year = "soon", month = 1, day = 1 })
failsWith('The call "fromFields" requires the "day" field.', datetime.fromFields, { year = 2026, month = 1 })
failsWith('The fields "year", "month" and "day" name no day of the calendar.', datetime.fromFields, { year = 2026, month = 2, day = 30 })

-- Parsing refuses a time of day or an offset outside its range, with a sentence that quotes the text.
failsWith('The text "2026-01-01T25:00:00Z" is not an ISO 8601 date and time.', datetime.parse, "2026-01-01T25:00:00Z")
failsWith("is not an ISO 8601 date and time.", datetime.parse, "2026-01-01T10:60:00Z")
failsWith("is not an ISO 8601 date and time.", datetime.parse, "2026-01-01T10:00:00+24:00")

-- An offset given to a method is bounded like the field of the same name.
failsWith("An offset must lie from -1439 to 1439 minutes", function() return now:iso(math.maxinteger) end)
failsWith("An offset must lie from -1439 to 1439 minutes", function() return now:fields(-1440) end)
failsWith("An offset must lie from -1439 to 1439 minutes", function() return now:format("%H", 100000) end)
assert(now:iso(-1439) == "2026-06-20T12:01:00-23:59", "The widest offset still renders, got " .. now:iso(-1439))

print("The \"datetime\" range tests passed.")
