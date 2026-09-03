# 🧾 json

A standalone JSON module that converts between JSON text and Lua values — backed by nlohmann/json.

```lua
local json = require("json")
print(json.encode({ name = "varn" }))
```

## Capabilities

| Function | What it does |
|---|---|
| `json.encode(value, options?)` | JSON text for any Lua value. `options.pretty = true` indents with two spaces, `options.indent = N` indents with `N`. Alias: `json.stringify`. |
| `json.decode(text)` | The Lua value for `text`. Raises on invalid input. Alias: `json.parse`. |

Type mapping: string, number (integer or float), boolean, and `nil` ↔ `null`. A sequence (contiguous `1..n` keys) ↔ a JSON array. Any other table ↔ a JSON object. Non-finite numbers (`NaN`/`Infinity`) encode as `null`, invalid UTF-8 is replaced rather than throwing, and decoding rejects deeply nested input and malformed text.

## Reference and tests

- Full reference: [docs/lua-api/json.md](../../docs/lua-api/json.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
