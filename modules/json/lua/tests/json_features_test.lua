-- covers type conversion, options, and aliases
local json = require("json")

-- encodes scalars to their canonical json text
assert(json.encode("hi") == '"hi"', "string scalar")
assert(json.encode(42) == "42", "integer scalar")
assert(json.encode(3.5) == "3.5", "float scalar")
assert(json.encode(3.0) == "3.0", "float keeps its type")
assert(json.encode(true) == "true", "true scalar")
assert(json.encode(false) == "false", "false scalar")
assert(json.encode(nil) == "null", "nil encodes as null")

-- encodes an empty table as an object and a sequence as an array
assert(json.encode({}) == "{}", "empty table is an object")
assert(json.encode({ 1, 2, 3 }) == "[1,2,3]", "sequence is an array")
assert(json.encode({ a = 1 }) == '{"a":1}', "map is an object")

-- encodes a table with non-contiguous integer keys as an object
assert(json.encode({ [1] = "a", [3] = "c" }) == '{"1":"a","3":"c"}', "sparse keys are an object")

-- encodes mixed nested objects and arrays in a single value
assert(json.encode({ a = { 1, 2, { x = true } } }) == '{"a":[1,2,{"x":true}]}', "mixed nesting")

-- decodes every scalar type back to lua
local v = json.decode('{"i":7,"f":1.5,"b":false,"s":"x","arr":[1,2,3],"obj":{"k":"v"}}')
assert(v.i == 7 and math.type(v.i) == "integer", "decode integer")
assert(v.f == 1.5 and math.type(v.f) == "float", "decode float")
assert(v.b == false, "decode boolean")
assert(v.s == "x", "decode string")
assert(v.arr[1] == 1 and v.arr[3] == 3, "decode array sequence")
assert(v.obj.k == "v", "decode nested object")

-- drops the key for a json null on decode
local n = json.decode('{"a":null,"b":1}')
assert(n.a == nil and n.b == 1, "null becomes nil")

-- preserves a structured value across encode and decode
local original = { name = "varn", count = 2, tags = { "fast", "small" }, meta = { ok = true } }
local back = json.decode(json.encode(original))
assert(back.name == "varn" and back.count == 2, "round-trip scalars")
assert(back.tags[1] == "fast" and back.tags[2] == "small", "round-trip array")
assert(back.meta.ok == true, "round-trip nested object")

-- checks stringify and parse as aliases of encode and decode
assert(json.stringify(5) == "5", "stringify alias encodes")
assert(json.parse("5") == 5, "parse alias decodes")
assert(json.stringify({ a = 1 }) == '{"a":1}', "stringify alias on a table")

-- indents with two spaces and inserts newlines when pretty is true
local pretty = json.encode({ a = 1 }, { pretty = true })
assert(pretty == "{\n  \"a\": 1\n}", "pretty uses two-space indent")

-- sets the number of leading spaces from an explicit indent
local indented = json.encode({ a = 1 }, { indent = 4 })
assert(indented == "{\n    \"a\": 1\n}", "indent of four spaces")

-- encodes non-finite numbers as null without throwing
assert(json.encode({ v = 1 / 0 }) == '{"v":null}', "positive infinity is null")
assert(json.encode({ v = -1 / 0 }) == '{"v":null}', "negative infinity is null")
assert(json.encode({ v = 0 / 0 }) == '{"v":null}', "nan is null")
assert(json.encode({ 1 / 0, -1 / 0, 0 / 0 }) == "[null,null,null]", "non-finite array is all null")

-- replaces invalid utf-8 without crashing the encoder
local replaced = json.encode("a" .. string.char(0xff) .. "b")
assert(type(replaced) == "string", "invalid utf-8 returns a string")
assert(replaced:sub(1, 2) == '"a' and replaced:sub(-2) == 'b"', "invalid utf-8 keeps the valid bytes")

-- raises on invalid input and pcall catches it
assert(not pcall(json.decode, "{not json"), "malformed input raises")
assert(not pcall(json.decode, ""), "empty input raises")
assert(not pcall(json.decode, "[1,2,"), "truncated input raises")

print("json features ok")
