# 🧾 json

A standalone JSON module that converts between JSON text and Lua values.

- `json.encode(value [, options])` → JSON text for any Lua value. Alias: `json.stringify`.
  - `options.pretty = true` indents with two spaces; `options.indent = N` indents with `N` spaces.
- `json.decode(text)` → the Lua value. Alias: `json.parse`. Raises on invalid input.

Type mapping: string, number (integer or float), boolean, `nil` ↔ `null`, a sequence
(contiguous `1..n` keys) ↔ a JSON array, any other table ↔ a JSON object. Non-finite
numbers (`NaN`/`Infinity`) encode as `null`, and invalid UTF-8 is replaced rather than
throwing. Decoding rejects deeply nested input and malformed text.

## Examples

### `encode_decode.lua`

```lua
-- json: encode a lua value to text and decode it back.
local json = require("json")

local text = json.encode({ name = "varn", version = "1.0", tags = { "fast", "small" } })
print(text)

local value = json.decode(text)
print(value.name, value.version, value.tags[1], value.tags[2])
```

### `pretty.lua`

```lua
-- json: pretty-print with a default or explicit indent.
local json = require("json")

print(json.encode({ user = { id = 1, roles = { "admin", "user" } } }, { pretty = true }))
print(json.encode({ a = 1, b = 2 }, { indent = 4 }))
```

### `types.lua`

```lua
-- json: type conversion between lua and json in both directions.
local json = require("json")

-- scalars and containers all encode directly.
print(json.encode("a string"))
print(json.encode(42))
print(json.encode(3.5))
print(json.encode(true))
print(json.encode({}))
print(json.encode({ 1, 2, 3 }))
print(json.encode({ nested = { a = 1, b = { 2, 3 } } }))

-- decode maps json types onto lua values.
local v = json.decode('{"i":7,"f":1.5,"b":false,"arr":[1,2],"obj":{"k":"v"}}')
print(v.i, v.f, v.b, v.arr[2], v.obj.k)
```

## Under the hood

Parsing and serialization use the nlohmann/json C++ library.
