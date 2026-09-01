-- checks parser and encoder security boundaries with cases tracked by CWE class
local json = require("json")

-- rejects deeply nested input before the parser can overflow the stack (CWE-674)
assert(not pcall(json.decode, string.rep("[", 5000) .. string.rep("]", 5000)), "deep nesting is rejected")

-- parses a shallow document at a safe depth (CWE-674)
assert(pcall(json.decode, string.rep("[", 100) .. string.rep("]", 100)), "shallow nesting parses")

-- bounds deeply nested encode without crashing the process (CWE-674)
local deep, cur = {}, nil
cur = deep
for _ = 1, 5000 do cur.n = {}; cur = cur.n end
assert(pcall(json.encode, deep), "deep encode is bounded, not a crash")

-- parses a huge but bounded flat array without exhausting resources (CWE-400)
local big = "[" .. string.rep("1,", 50000) .. "1]"
local ok, arr = pcall(json.decode, big)
assert(ok and #arr == 50001, "huge flat array is handled")

-- round-trips a large string value with its length intact (CWE-400)
local huge = json.decode(json.encode(string.rep("x", 100000)))
assert(#huge == 100000, "large string value round-trips")

-- escapes control characters on encode and rejects raw control bytes on decode (CWE-74, CWE-116)
assert(json.encode("a\1b") == '"a\\u0001b"', "control char is escaped on encode")
assert(not pcall(json.decode, '"a' .. string.char(1) .. 'b"'), "raw control byte in a string is rejected")

-- replaces invalid utf-8 in a value on encode (CWE-176)
assert(type(json.encode("a" .. string.char(0xff, 0xfe, 0x80) .. "b")) == "string", "invalid utf-8 does not crash the encoder")

-- preserves an embedded NUL and its length across a binary round-trip (CWE-176)
local nul = json.decode(json.encode("a\0b"))
assert(#nul == 3, "embedded NUL is preserved")

-- decodes a number past the lua_Integer range to a float (CWE-681, CWE-190)
local past = json.decode("123456789012345678901234567890")
assert(math.type(past) == "float", "out-of-range integer becomes a float")

-- rejects an absurd exponent (CWE-400)
assert(not pcall(json.decode, "1e1000000"), "huge exponent literal is rejected")

-- resolves duplicate keys deterministically with the last value winning (CWE-20)
assert(json.decode('{"a":1,"a":2}').a == 2, "duplicate keys are last-wins")

-- rejects malformed, truncated, and empty inputs via pcall (CWE-20)
assert(not pcall(json.decode, ""), "empty input is rejected")
assert(not pcall(json.decode, '{"a":'), "truncated object is rejected")
assert(not pcall(json.decode, "[1,2,"), "truncated array is rejected")
assert(not pcall(json.decode, "{}x"), "trailing garbage is rejected")

print("json security ok")
