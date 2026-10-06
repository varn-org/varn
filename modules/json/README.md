# 🧾 json

A standalone JSON module that converts between JSON text and Lua values — parsed with the event interface of nlohmann/json straight into Lua tables and written straight into a Lua string buffer.

```lua
local json = require("json")
print(json.encode({ name = "varn" }))
```

## Capabilities

| Function | What it does |
|---|---|
| `json.encode(value, options?)` | JSON text for any Lua value. `options.pretty = true` indents with two spaces and `options.indent = N` with `N`, listing keys in sorted order. Raises with the key of a function, a userdata or a table that contains itself. Alias: `json.stringify`. |
| `json.decode(text, options?)` | The Lua value for `text`. `options.nullValue` stands for every `null`. Raises on invalid input with the line and the column. Alias: `json.parse`. |
| `json.null` | A sentinel that encodes as `null` and that `{ nullValue = json.null }` decodes to. |
| `json.array(t?)` | Marks a table so it encodes as an array, empty or with holes as `null`. |
| `json.object(t?)` | Marks a table so it encodes as an object, empty or a sequence. |

Type mapping: string, number (integer or float), boolean, and `nil` ↔ `null`. A sequence (contiguous `1..n` keys) ↔ a JSON array. Any other table ↔ a JSON object. Decoded arrays carry the array marker, so they encode back as arrays. Non-finite numbers (`NaN`/`Infinity`) encode as `null`, invalid UTF-8 is replaced rather than throwing, and decoding rejects deeply nested input and malformed text.

## Reference and tests

- Full reference: [docs/lua-api/json.md](../../docs/lua-api/json.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
