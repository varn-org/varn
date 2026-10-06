-- Covers the errors of encode and decode, which name the key or the line and column, and the exact text the encoder writes.
local json = require("json")

local function failure(fn, ...)
    local ok, err = pcall(fn, ...)
    assert(not ok, "The call was expected to fail")
    return err
end

local function expectError(expected, fn, ...)
    local err = failure(fn, ...)
    assert(err:find(expected, 1, true), 'Expected "' .. expected .. '" in "' .. err .. '"')
end

-- Names the key of a value JSON cannot carry.
expectError('[JsonModule] The value at key "f" is a function and cannot be encoded.', json.encode, { f = print })
expectError('The value at key "a.b[3]" is a function and cannot be encoded.', json.encode, { a = { b = { 1, 2, print } } })
expectError('The value at key "[1]" is a userdata and cannot be encoded.', json.encode, { io.stdout })
expectError('The value at key "co" is a thread and cannot be encoded.', json.encode, { co = coroutine.create(print) })
expectError("The value is a function and cannot be encoded.", json.encode, print)

-- Names the key where a table reaches itself.
local cyclic = {}
cyclic.self = cyclic
expectError('The table at key "self" contains itself and cannot be encoded.', json.encode, cyclic)
local parent = { items = {} }
parent.items[1] = { owner = parent }
expectError('The table at key "items[1].owner" contains itself and cannot be encoded.', json.encode, parent)
expectError('The table at key "items[1].owner" contains itself and cannot be encoded.', json.encode, parent, { pretty = true })

-- Encodes a table that appears twice without reaching itself.
local shared = { 1 }
assert(json.decode(json.encode({ a = shared, b = shared })).b[1] == 1, "A shared table is not a cycle")

-- Refuses a key JSON cannot carry and accepts number keys as strings.
expectError("The table has a key of type boolean and cannot be encoded.", json.encode, { [true] = 1 })
expectError('The table at key "t" has a key of type table and cannot be encoded.', json.encode, { t = { [{}] = 1 } }, { indent = 2 })
assert(json.encode({ [1.5] = 1 }) == '{"1.5":1}', "A float key is written as a string")
assert(json.encode({ [-2] = 1 }) == '{"-2":1}', "A negative integer key is written as a string")

-- Masks control characters and shortens a long key in a message.
expectError('The value at key "a?b" is a function', json.encode, { ["a\nb"] = print })
local long = string.rep("k", 500)
local longErr = failure(json.encode, { [long] = print })
assert(#longErr < 400 and longErr:find('"...k', 1, true), longErr)

-- Names the line and the column of invalid text, counted in bytes from one.
expectError("[JsonModule] The input is not valid JSON at line 1, column 3.", json.decode, "{not json")
expectError("The input is not valid JSON at line 1, column 1.", json.decode, "")
expectError("The input is not valid JSON at line 1, column 3.", json.decode, "{}x")
expectError("The input is not valid JSON at line 1, column 6.", json.decode, "[1,2,")
expectError("The input is not valid JSON at line 3, column 11.", json.decode, '{\n  "a": 1,\n  "b": nul\n}')
expectError("The input nests deeper than 200 levels at line 1, column 201.", json.decode, string.rep("[", 300))
expectError("The input nests deeper than 200 levels at line 2, column 200.", json.decode, '["[\\"",\n' .. string.rep("[", 300))
expectError('The number "1e400" at line 1, column 6 is out of range.', json.decode, "[1e400]")

-- Keeps integers past the signed range as floats.
assert(math.type(json.decode("9223372036854775807")) == "integer", "The largest integer stays an integer")
assert(math.type(json.decode("9223372036854775808")) == "float", "An integer past the signed range becomes a float")
assert(math.type(json.decode("-9223372036854775808")) == "integer", "The smallest integer stays an integer")
assert(json.encode(math.maxinteger) == "9223372036854775807", "The largest integer encodes exactly")
assert(json.encode(math.mininteger) == "-9223372036854775808", "The smallest integer encodes exactly")

-- Writes the shortest text that reads back as the same float.
assert(json.encode(0.1) == "0.1", "A decimal float")
assert(json.encode(1e300) == "1e+300", "A large float")
assert(json.encode(-0.0) == "-0.0", "A negative zero")
assert(json.encode(100.0) == "100.0", "An integral float keeps its point")
assert(json.decode(json.encode(1 / 3)) == 1 / 3, "A float round-trips exactly")

-- Escapes what JSON requires and keeps valid UTF-8 as it is.
assert(json.encode('a"b\\c') == '"a\\"b\\\\c"', "Quotes and backslashes are escaped")
assert(json.encode("\n\r\t\b\f") == '"\\n\\r\\t\\b\\f"', "Short escapes are used")
assert(json.encode("\31\127") == '"\\u001f\127"', "Other control characters use the long escape")
assert(json.encode("ação €𝄞") == '"ação €𝄞"', "Valid UTF-8 passes through")
assert(json.encode("/") == '"/"', "A slash is not escaped")

-- Replaces each invalid UTF-8 sequence with one replacement character.
local replacement = "\u{FFFD}"
assert(json.encode("a\255b") == '"a' .. replacement .. 'b"', "A stray byte is replaced")
assert(json.encode("a\226\130") == '"a' .. replacement .. '"', "A truncated sequence is replaced once")
assert(json.encode("\226\130x") == '"' .. replacement .. 'x"', "A broken sequence keeps the byte that broke it")
assert(json.encode("\237\160\128") == '"' .. string.rep(replacement, 3) .. '"', "An encoded surrogate is replaced")
assert(json.encode("\192\175") == '"' .. string.rep(replacement, 2) .. '"', "An overlong form is replaced")

-- Indents nested containers, sorts keys and keeps empty containers on one line.
local pretty = json.encode({ b = 1, a = { 2, {} }, c = { z = json.array(), y = "x" } }, { pretty = true })
assert(pretty == '{\n  "a": [\n    2,\n    {}\n  ],\n  "b": 1,\n  "c": {\n    "y": "x",\n    "z": []\n  }\n}', pretty)
assert(json.encode({ 1, { 2 } }, { indent = 1 }) == "[\n 1,\n [\n  2\n ]\n]", "An explicit indent")
assert(json.encode({ [2] = "b", [10] = "j", a = 1 }, { pretty = true }) == '{\n  "10": "j",\n  "2": "b",\n  "a": 1\n}', "Keys sort as text")
assert(json.encode("x", { pretty = true }) == '"x"', "A scalar has no indentation")
assert(json.encode({ a = 1 }, { indent = 0 }) == '{"a":1}', "An indent of zero is compact")

-- Encodes deep nesting without recursion and reads it back as the same text.
local deep, cur = {}, nil
cur = deep
for _ = 1, 150 do cur.n = {}; cur = cur.n end
local deepText = json.encode(deep)
assert(json.encode(json.decode(deepText)) == deepText, "A deep value round-trips")

print("The \"json\" error and format tests passed.")
