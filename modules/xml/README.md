# 🧬 xml

A standalone XML module that converts between XML text and a Lua node model — parsing and serialization backed by pugixml.

```lua
local xml = require("xml")
print(xml.encode({ name = "tag", text = "varn" }))
```

## Capabilities

| Function | What it does |
|---|---|
| `xml.encode(node, options?)` | Serialize a node table to XML text. `options.pretty = true` indents with two spaces and `options.indent = N` indents with `N` spaces, from 0 to 16. Raises with the element and attribute of a value or a name XML cannot carry. |
| `xml.decode(text)` | Parse XML text into the root node table. Raises on invalid input. |

The node model round-trips losslessly: `name` (element name), `attributes` (present only when the element has attributes), `children` (child elements in document order), and `text` (concatenated direct text). Text is escaped on encode, and a name that is not an XML name or text that XML cannot carry raises instead of being rewritten. The parser does not load external entities or expand DTD entities, and nesting deeper than 256 elements raises an error on both sides.

## Reference and tests

- Full reference: [docs/lua-api/xml.md](../../docs/lua-api/xml.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
