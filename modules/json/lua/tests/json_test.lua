-- covers type conversion, options, and security checks
local json = require("json")

-- converts types in both directions
assert(json.encode("hi") == '"hi"', "string scalar")
assert(json.encode(42) == "42", "int scalar")
assert(json.encode(true) == "true", "bool scalar")
assert(json.encode(false) == "false", "false scalar")
assert(json.encode(nil) == "null", "nil scalar")
assert(json.encode({}) == "{}", "empty table -> object")
assert(json.encode({ 1, 2, 3 }) == "[1,2,3]", "sequence -> array")
assert(json.encode({ a = 1 }) == '{"a":1}', "map -> object")

local v = json.decode('{"n":7,"arr":[1,2,3],"obj":{"k":"v"},"b":false}')
assert(v.n == 7 and v.arr[3] == 3 and v.obj.k == "v" and v.b == false, "decode nested")

-- checks pretty, indent, and aliases
assert(json.encode({ a = 1 }, { pretty = true }):find("\n"), "pretty has newline")
assert(json.encode({ a = 1 }, { indent = 4 }):find("    ", 1, true), "indent of 4 spaces")
assert(json.stringify(5) == "5" and json.parse("5") == 5, "stringify/parse aliases")

-- encodes nan and infinity as null without throwing
assert(json.encode({ v = 1 / 0 }) == '{"v":null}', "inf -> null")
assert(json.encode({ v = -1 / 0 }) == '{"v":null}', "-inf -> null")
assert(json.encode({ v = 0 / 0 }) == '{"v":null}', "nan -> null")

-- encodes invalid utf-8 without crashing the encoder
assert(type(json.encode({ b = string.char(0xff, 0xfe, 0x80) })) == "string", "invalid utf-8 no crash")

-- round-trips an embedded NUL safely
local nul = json.decode(json.encode("a\0b"))
assert(#nul == 3, "NUL preserved (len " .. #nul .. ")")

-- rejects deeply nested input before the parser can overflow the stack
assert(not pcall(json.decode, string.rep("[", 5000) .. string.rep("]", 5000)), "deep nesting rejected")

-- bounds deeply nested encode without crashing
local t, cur = {}, nil
cur = t
for _ = 1, 5000 do cur.n = {}; cur = cur.n end
assert(pcall(json.encode, t), "deep encode bounded")

-- rejects malformed, truncated, and empty input
assert(not pcall(json.decode, "{not json"), "malformed rejected")
assert(not pcall(json.decode, "[1,2,"), "truncated rejected")
assert(not pcall(json.decode, ""), "empty rejected")

-- resolves duplicate keys deterministically with the last winning
assert(json.decode('{"a":1,"a":2}').a == 2, "duplicate keys last-wins")

-- handles a large flat array
assert(json.decode("[" .. string.rep("1,", 10000) .. "1]")[1] == 1, "large flat array")

print("json ok")
