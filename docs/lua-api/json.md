# 🧾 json

A standalone JSON module that converts between JSON text and Lua values.

- `json.encode(value [, options])` → JSON text for any Lua value. Alias: `json.stringify`.
  - `options.pretty = true` indents with two spaces. `options.indent = N` indents with `N` spaces.
- `json.decode(text)` → the Lua value. Alias: `json.parse`. Raises on invalid input.

Type mapping: string, number (integer or float), boolean, `nil` ↔ `null`, a sequence (contiguous `1..n` keys) ↔ a JSON array, any other table ↔ a JSON object. Non-finite numbers (`NaN`/`Infinity`) encode as `null`, and invalid UTF-8 is replaced rather than throwing. Decoding rejects deeply nested input and malformed text.

## Examples

### `aliases.lua`

```lua
-- Shows stringify and parse as aliases of encode and decode.
local json = require("json")

local text = json.stringify({ id = 1, active = true })
print(text)

local value = json.parse(text)
print(value.id, value.active)
```

### `decode_error.lua`

```lua
-- Guards decode with pcall since invalid input raises.
local json = require("json")

local ok, err = pcall(json.decode, "{not json")
print(ok, err)

local value = json.decode('{"valid":true}')
print(value.valid)
```

### `encode_decode.lua`

```lua
-- Encodes a lua value to text and decodes it back.
local json = require("json")

local text = json.encode({ name = "varn", version = "1.0", tags = { "fast", "small" } })
print(text)

local value = json.decode(text)
print(value.name, value.version, value.tags[1], value.tags[2])
```

### `non_finite.lua`

```lua
-- Encodes non-finite numbers as null instead of throwing.
local json = require("json")

-- Encodes nan and both infinities as null.
print(json.encode({ nan = 0 / 0, pos = 1 / 0, neg = -1 / 0 }))

-- Keeps finite values in a mixed array and nulls out the rest.
print(json.encode({ 1, 1 / 0, 2.5, 0 / 0 }))
```

### `pretty.lua`

```lua
-- Pretty-prints with a default or explicit indent.
local json = require("json")

print(json.encode({ user = { id = 1, roles = { "admin", "user" } } }, { pretty = true }))
print(json.encode({ a = 1, b = 2 }, { indent = 4 }))
```

### `types.lua`

```lua
-- Converts types between lua and json in both directions.
local json = require("json")

-- Encodes scalars and containers directly.
print(json.encode("a string"))
print(json.encode(42))
print(json.encode(3.5))
print(json.encode(true))
print(json.encode({}))
print(json.encode({ 1, 2, 3 }))
print(json.encode({ nested = { a = 1, b = { 2, 3 } } }))

-- Decodes json types onto lua values.
local v = json.decode('{"i":7,"f":1.5,"b":false,"arr":[1,2],"obj":{"k":"v"}}')
print(v.i, v.f, v.b, v.arr[2], v.obj.k)
```
## Under the hood

Parsing and serialization use the nlohmann/json C++ library.
