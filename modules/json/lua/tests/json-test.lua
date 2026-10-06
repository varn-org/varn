-- Covers type conversion, options, and security checks.
local json = require("json")

-- Converts types in both directions.
assert(json.encode("hi") == '"hi"', "String scalar")
assert(json.encode(42) == "42", "Integer scalar")
assert(json.encode(true) == "true", "Boolean scalar")
assert(json.encode(false) == "false", 'The "false" scalar')
assert(json.encode(nil) == "null", 'The "nil" scalar')
assert(json.encode({}) == "{}", "Empty table -> object")
assert(json.encode({ 1, 2, 3 }) == "[1,2,3]", "Sequence -> array")
assert(json.encode({ a = 1 }) == '{"a":1}', "Map -> object")

local v = json.decode('{"n":7,"arr":[1,2,3],"obj":{"k":"v"},"b":false}')
assert(v.n == 7 and v.arr[3] == 3 and v.obj.k == "v" and v.b == false, "Decode nested")

-- Checks `pretty`, `indent`, and the aliases.
assert(json.encode({ a = 1 }, { pretty = true }):find("\n"), 'The option "pretty" has a newline')
assert(json.encode({ a = 1 }, { indent = 4 }):find("    ", 1, true), "Indent of 4 spaces")
assert(json.stringify(5) == "5" and json.parse("5") == 5, 'The "stringify" and "parse" aliases')

-- Encodes NaN and infinity as null without throwing.
assert(json.encode({ v = 1 / 0 }) == '{"v":null}', "Infinity -> null")
assert(json.encode({ v = -1 / 0 }) == '{"v":null}', "Negative infinity -> null")
assert(json.encode({ v = 0 / 0 }) == '{"v":null}', "NaN -> null")

-- Encodes invalid UTF-8 without crashing the encoder.
assert(type(json.encode({ b = string.char(0xff, 0xfe, 0x80) })) == "string", "Invalid UTF-8 does not crash")

-- Round-trips an embedded NUL safely.
local nul = json.decode(json.encode("a\0b"))
assert(#nul == 3, "NUL preserved (length " .. #nul .. ")")

-- Rejects deeply nested input before the parser can overflow the stack.
assert(not pcall(json.decode, string.rep("[", 5000) .. string.rep("]", 5000)), "Deep nesting rejected")

-- Encodes deeply nested values whole without crashing.
local t, cur = {}, nil
cur = t
for _ = 1, 5000 do cur.n = {}; cur = cur.n end
assert(pcall(json.encode, t), "Deep encode bounded")

-- Rejects malformed, truncated, and empty input.
assert(not pcall(json.decode, "{not json"), "Malformed rejected")
assert(not pcall(json.decode, "[1,2,"), "Truncated rejected")
assert(not pcall(json.decode, ""), "Empty rejected")

-- Resolves duplicate keys deterministically with the last winning.
assert(json.decode('{"a":1,"a":2}').a == 2, "Duplicate keys last-wins")

-- Handles a large flat array.
assert(json.decode("[" .. string.rep("1,", 10000) .. "1]")[1] == 1, "Large flat array")

print("The \"json\" tests passed.")
