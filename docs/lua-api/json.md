# 🧾 json

A standalone JSON module that converts between JSON text and Lua values.

- `json.encode(value [, options])` → JSON text for any Lua value. Alias: `json.stringify`.
  - Setting `options.pretty = true` indents with two spaces. Setting `options.indent = N` indents with `N` spaces, from 0 to 16. Indented output lists the keys of every object in sorted order.
  - Raises an error that names the key of a value JSON cannot carry: a function, a userdata, a thread, a table that contains itself, or a key that is not a string or a number.
- `json.decode(text [, options])` → The Lua value. Alias: `json.parse`. Raises on invalid input with the line and the column, counted in bytes from one.
  - Setting `options.nullValue` decodes every JSON `null` as that value instead of `nil`, so `{ nullValue = json.null }` keeps the keys of objects and the length of arrays.
- `json.null` → A sentinel that encodes as `null`. It is a light userdata, distinct from `nil` and from every other value.
- `json.array([t])` → Marks `t`, or a new table, so it encodes as an array: an empty table becomes `[]` and every hole up to the largest key becomes `null`. A key that is not a positive integer raises on encode. Answers the table.
- `json.object([t])` → Marks `t`, or a new table, so it encodes as an object even when it is empty or a sequence. Answers the table.

Type mapping: string, number (integer or float), boolean, `nil` ↔ `null`, a sequence (contiguous `1..n` keys) ↔ a JSON array, any other table ↔ a JSON object. A number key becomes a string key. Every decoded array carries the array marker, so it encodes back as an array even when it is empty or has holes. A marker is a metatable, so `json.array` and `json.object` refuse a table that already has a metatable of its own. Non-finite numbers (`NaN`/`Infinity`) encode as `null`, and each invalid UTF-8 sequence is replaced with U+FFFD rather than throwing. An integer past the range of a Lua integer decodes as a float, and a number past the range of a float raises. Decoding rejects input nested deeper than 200 levels and malformed text, while encoding takes any depth.

## Examples

### Aliases

```lua
-- Shows stringify and parse as aliases of encode and decode.
local json = require("json")

local text = json.stringify({ id = 1, active = true })
print(text)

local value = json.parse(text)
print(value.id, value.active)
```

### Decode error

```lua
-- Guards decode with pcall since invalid input raises with the line and the column.
local json = require("json")

local ok, err = pcall(json.decode, '{\n  "a": 1,\n  "b": nul\n}')
print(ok, err)

local value = json.decode('{"valid":true}')
print(value.valid)
```

### Encode error

```lua
-- Names the key of a value JSON cannot carry.
local json = require("json")

local ok, err = pcall(json.encode, { user = { name = "ana", greet = print } })
print(ok, err)

local node = { name = "root" }
node.self = node
print(pcall(json.encode, node))
```

### Encode and decode

```lua
-- Encodes a Lua value to text and decodes it back.
local json = require("json")

local text = json.encode({ name = "varn", version = "1.0", tags = { "fast", "small" } })
print(text)

local value = json.decode(text)
print(value.name, value.version, value.tags[1], value.tags[2])
```

### Null

```lua
-- Keeps null through a decode and an encode with the sentinel.
local json = require("json")

local value = json.decode('{"name":null,"scores":[1,null,3]}', { nullValue = json.null })
print(value.name == json.null, #value.scores)
print(json.encode(value))

-- Writes null where a key must stay.
print(json.encode({ deleted = json.null }))
```

### Arrays and objects

```lua
-- Marks tables whose shape the keys alone cannot tell.
local json = require("json")

print(json.encode({ items = json.array() }))
print(json.encode(json.array({ 1, nil, 3 })))
print(json.encode(json.object({ "a", "b" })))

-- Keeps an empty decoded array an array.
print(json.encode(json.decode('{"items":[]}')))
```

### Non-finite numbers

```lua
-- Encodes non-finite numbers as null instead of throwing.
local json = require("json")

-- Encodes NaN and both infinities as null.
print(json.encode({ nan = 0 / 0, pos = 1 / 0, neg = -1 / 0 }))

-- Keeps finite values in a mixed array and nulls out the rest.
print(json.encode({ 1, 1 / 0, 2.5, 0 / 0 }))
```

### Pretty

```lua
-- Pretty-prints with a default or explicit indent and sorted keys.
local json = require("json")

print(json.encode({ user = { id = 1, roles = { "admin", "user" } } }, { pretty = true }))
print(json.encode({ b = 2, a = 1 }, { indent = 4 }))
```

### Types

```lua
-- Converts types between Lua and JSON in both directions.
local json = require("json")

-- Encodes scalars and containers directly.
print(json.encode("a string"))
print(json.encode(42))
print(json.encode(3.5))
print(json.encode(true))
print(json.encode({}))
print(json.encode({ 1, 2, 3 }))
print(json.encode({ nested = { a = 1, b = { 2, 3 } } }))

-- Decodes JSON types onto Lua values.
local v = json.decode('{"i":7,"f":1.5,"b":false,"arr":[1,2],"obj":{"k":"v"}}')
print(v.i, v.f, v.b, v.arr[2], v.obj.k)
```
## Under the hood

Decoding parses the text in place with the event interface of the nlohmann/json C++ library and builds the Lua tables as the events arrive, with no intermediate document. Encoding walks the tables without recursion and writes straight into a Lua string buffer, formatting numbers and escaping strings itself, and indents in the same pass.
