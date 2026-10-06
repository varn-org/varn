-- The indentation of "encode" is a whole number of spaces within its range, refused by name rather than bent into it.
local json = require("json")

local function failsWith(fragment, fn, ...)
    local ok, err = pcall(fn, ...)
    assert(not ok, "The call should raise an error containing: " .. fragment)
    assert(tostring(err):find(fragment, 1, true), "The error should contain '" .. fragment .. "', got " .. tostring(err))
end

local message = 'The option "indent" must be a whole number of spaces from 0 to 16.'
failsWith(message, json.encode, { a = 1 }, { indent = 17 })
failsWith(message, json.encode, { a = 1 }, { indent = -1 })
failsWith(message, json.encode, { a = 1 }, { indent = 2.5 })
failsWith(message, json.encode, { a = 1 }, { indent = "4" })

assert(json.encode({ a = 1 }, { indent = 16 }) == '{\n' .. string.rep(" ", 16) .. '"a": 1\n}', "The widest indent is accepted")
assert(json.encode({ a = 1 }, { indent = 2.0 }) == '{\n  "a": 1\n}', "A float with a whole value is accepted")

print("The \"json\" option tests passed.")
