# 🧬 xml

A standalone XML module that converts between XML text and a Lua node model.

- `xml.encode(node [, options])` → XML text from a node table.
  - Setting `options.pretty = true` indents with two spaces. Setting `options.indent = N` indents with `N` spaces, a whole number from 0 to 16. Any other width raises an error that names the option.
- `xml.decode(text)` → The root node table. Raises on invalid input.

The node model round-trips losslessly:

```lua
{
  name = "tag",                 -- Element name
  attributes = { k = "v" },     -- Present only when the element has attributes
  children = { <node>, ... },   -- Child elements, in document order
  text = "...",                 -- Concatenated direct text, when present
}
```

Text is escaped on encode. A `name` and an attribute name are strings that are names of XML 1.0, and an element or attribute name that is not one, such as `bad name!` or `1abc`, raises `The element name "bad name!" is not a valid XML name.` rather than being rewritten. An attribute value and `text` must be valid UTF-8 of the characters XML 1.0 allows, so a control character such as U+0001 raises `The text of the element "a" holds the character U+0001, which XML cannot carry.` and invalid UTF-8 raises `The text of the element "a" is not valid UTF-8.`, rather than being dropped. An attribute value and `text` are a string, a number or a boolean, written as their text. Any other value, such as a table or a function, raises an error that names the element and the attribute it sits in, and so does a node without a `name` or a child that is not a node table. The parser does not load external entities or expand DTD entities. A document or a node tree that nests deeper than 256 elements raises `The XML document nests deeper than 256 elements.` or `The node tree nests deeper than 256 elements.` instead of losing the deeper elements.

## Examples

### Encode and decode

```lua
-- Build a document from the node model and parse it back.
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

### Escaping

```lua
-- Text and attribute specials are escaped, and an invalid name is refused.
local xml = require("xml")

-- Special characters in text and attributes are escaped on encode.
print(xml.encode({ name = "msg", attributes = { note = 'a & b "c"' }, text = "x < y > z" }))

-- An invalid element name raises rather than being rewritten.
print(pcall(xml.encode, { name = "bad name!", text = "ok" }))

-- Escaped values round-trip back to their original form.
local back = xml.decode(xml.encode({ name = "n", text = "a < b & c" }))
print(back.text)
```

### Indent

```lua
-- Indent sets the indentation width in spaces.
local xml = require("xml")

local node = { name = "root", children = { { name = "item", text = "value" } } }
print(xml.encode(node, { indent = 4 }))
```

### Roundtrip

```lua
-- Parse a document, walk its nodes, and re-encode it.
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
