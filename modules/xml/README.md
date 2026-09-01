# 🧬 xml

A standalone XML module that converts between XML text and a Lua node model — parsing and serialization
backed by pugixml.

```lua
local xml = require("xml")
print(xml.encode({ name = "tag", text = "varn" }))
```

## Capabilities

| Function | What it does |
|---|---|
| `xml.encode(node, options?)` | Serialize a node table to XML text; `options.pretty = true` indents with two spaces and `options.indent = N` indents with `N` spaces. Alias: `xml.stringify`. |
| `xml.decode(text)` | Parse XML text into the root node table; raises on invalid input. Alias: `xml.parse`. |

The node model round-trips losslessly: `name` (element name), `attributes` (present only when the
element has attributes), `children` (child elements in document order), and `text` (concatenated direct
text). Element names are sanitized and text is escaped on encode. The parser does not load external
entities or expand DTD entities, and node conversion is depth-capped.

## Reference, examples, and tests

- Full reference: [docs/lua-api/xml.md](../../docs/lua-api/xml.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
