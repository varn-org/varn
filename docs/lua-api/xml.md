# 🧬 xml

A standalone XML module that converts between XML text and a Lua node model.

- `xml.encode(node [, options])` → XML text from a node table. Alias: `xml.stringify`.
  - `options.pretty = true` indents with two spaces; `options.indent = N` indents with `N` spaces.
- `xml.decode(text)` → the root node table. Alias: `xml.parse`. Raises on invalid input.

The node model round-trips losslessly:

```lua
{
  name = "tag",                 -- element name
  attributes = { k = "v" },     -- present only when the element has attributes
  children = { <node>, ... },   -- child elements, in document order
  text = "...",                 -- concatenated direct text, when present
}
```

Element names are sanitized and text is escaped on encode. The parser does not load
external entities or expand DTD entities, and node conversion is depth-capped.

## Examples

### `encode_decode.lua`

```lua
-- xml: build a document from the node model and parse it back.
local xml = require("xml")

local doc = xml.encode({
    name = "catalog",
    children = {
        {
            name = "book",
            attributes = { id = "1" },
            children = { { name = "title", text = "Varn" } },
        },
    },
}, { pretty = true })
print(doc)

local node = xml.decode(doc)
print(node.name, node.children[1].attributes.id, node.children[1].children[1].text)
```

### `roundtrip.lua`

```lua
-- xml: parse a document, walk its nodes, and re-encode it.
local xml = require("xml")

local original = '<note priority="high"><to>Lua</to><from>C++</from><body>hello &amp; bye</body></note>'

local node = xml.decode(original)
print("root:", node.name, "priority:", node.attributes.priority)
for _, child in ipairs(node.children) do
    print(child.name, "=", child.text)
end

print(xml.encode(node))
```

## Under the hood

Parsing and serialization use the pugixml C++ library.
