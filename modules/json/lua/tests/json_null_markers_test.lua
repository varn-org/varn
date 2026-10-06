-- Covers `json.null`, the option `nullValue`, the markers `json.array` and `json.object`, and how decoded arrays encode back.
local json = require("json")

-- Encodes the sentinel as null wherever it sits.
assert(json.encode(json.null) == "null", 'The sentinel "json.null" encodes as null')
assert(json.encode({ a = json.null }) == '{"a":null}', "A null value keeps its key")
assert(json.encode({ 1, json.null, 3 }) == "[1,null,3]", "A null element keeps its place")
assert(type(json.null) == "userdata" and json.null ~= nil, "The sentinel is a value of its own")

-- Drops a null by default and keeps it as the value of `nullValue` when given.
local holes = json.decode("[1,null,3]")
assert(holes[1] == 1 and holes[2] == nil and holes[3] == 3, "A null leaves a hole by default")
local kept = json.decode("[1,null,3]", { nullValue = json.null })
assert(#kept == 3 and kept[2] == json.null, 'The option "nullValue" keeps the length of an array')
local object = json.decode('{"a":null,"b":1}', { nullValue = json.null })
assert(object.a == json.null and object.b == 1, 'The option "nullValue" keeps the key of an object')
assert(json.decode("null", { nullValue = json.null }) == json.null, "A null at the root becomes the value given")
assert(json.decode('{"a":null}', { nullValue = false }).a == false, "Any value can stand for null")
assert(json.decode('{"a":null}', {}).a == nil, "An empty options table keeps the default")
assert(not pcall(json.decode, "[]", "nope"), "Options that are not a table are refused")

-- Round-trips null exactly through the sentinel.
assert(json.encode(json.decode('{"a":[null,1]}', { nullValue = json.null })) == '{"a":[null,1]}', "Null round-trips")

-- Encodes the holes of a decoded array as null, since every decoded array carries the array marker.
assert(json.encode(json.decode("[1,null,3]")) == "[1,null,3]", "A decoded array keeps its holes as null")
assert(json.encode(json.decode("[]")) == "[]", "An empty decoded array stays an array")
assert(json.encode(json.decode('{"list":[]}')) == '{"list":[]}', "A nested empty array stays an array")
assert(json.encode(json.decode("{}")) == "{}", "An empty decoded object stays an object")

-- Marks a table as an array, so an empty or sparse table encodes as one.
assert(json.encode(json.array()) == "[]", 'The call "json.array()" answers an empty array')
assert(json.encode(json.array({})) == "[]", "An empty marked table is an array")
assert(json.encode(json.array({ 1, nil, 3 })) == "[1,null,3]", "The holes of a marked array are null")
assert(json.encode(json.array({ [2] = "b" })) == '[null,"b"]', "A marked array runs to its largest key")
local same = {}
assert(json.array(same) == same, "The marker answers the table it marked")

-- Marks a table as an object, so a sequence encodes as one.
assert(json.encode(json.object()) == "{}", 'The call "json.object()" answers an empty object')
assert(json.encode(json.object({ "a", "b" })) == '{"1":"a","2":"b"}', "A marked sequence is an object")

-- Switches a marker and refuses a table that has a metatable of its own.
local switched = json.object({ 1 })
json.array(switched)
assert(json.encode(switched) == "[1]", "A marked table can be marked again")
local ok, err = pcall(json.array, setmetatable({}, {}))
assert(not ok and err:find("The table already has a metatable, so it cannot be marked as a JSON array or object.", 1, true), err)
assert(not pcall(json.object, 42), "A marker refuses a value that is not a table")

-- Refuses a key no array can hold and names it with the path of the table.
local ok2, err2 = pcall(json.encode, { list = json.array({ 1, x = 2 }) })
assert(not ok2 and err2:find('The table at key "list" is marked as an array but holds the key "x".', 1, true), err2)
local ok3, err3 = pcall(json.encode, json.array({ [0] = 1 }))
assert(not ok3 and err3:find("The table is marked as an array but holds the key 0.", 1, true), err3)
local ok4, err4 = pcall(json.encode, json.array({ [1.5] = 1 }))
assert(not ok4 and err4:find("The table is marked as an array but holds the key 1.5.", 1, true), err4)

-- Keeps a plain table encoding as before, a sequence as an array and anything else as an object.
assert(json.encode({ [1] = "a", [3] = "c" }) == '{"1":"a","3":"c"}', "An unmarked sparse table is an object")
assert(json.encode({}) == "{}", "An unmarked empty table is an object")

print("The \"json\" null and marker tests passed.")
