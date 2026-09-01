-- feature coverage for the node model, options, and aliases
local xml = require("xml")

-- decode exposes name, attributes, ordered children, and text
local node = xml.decode('<root id="1" lang="en"><item>hi</item><item>bye</item></root>')
assert(node.name == "root", "root name is decoded")
assert(node.attributes.id == "1" and node.attributes.lang == "en", "attributes are decoded")
assert(#node.children == 2, "children count is correct")
assert(node.children[1].text == "hi" and node.children[2].text == "bye", "child text is decoded")

-- children keep document order even with repeated names
local ordered = xml.decode("<r><a>1</a><b>2</b><a>3</a></r>")
assert(ordered.children[1].name == "a" and ordered.children[1].text == "1", "first child in order")
assert(ordered.children[2].name == "b" and ordered.children[2].text == "2", "second child in order")
assert(ordered.children[3].name == "a" and ordered.children[3].text == "3", "third child in order")

-- attributes are present only when the element actually has them
local noattrs = xml.decode("<root><a>x</a></root>")
assert(noattrs.attributes == nil, "no attributes table when none are set")
assert(noattrs.children[1].attributes == nil, "child has no attributes table when none are set")

-- encode produces a prolog and the element with its attributes
local doc = xml.encode({ name = "doc", attributes = { v = "1" }, children = { { name = "x", text = "hello" } } })
assert(doc:find("<?xml", 1, true), "encode emits a prolog")
assert(doc:find("<doc", 1, true) and doc:find('v="1"', 1, true), "encode emits the element and its attribute")

-- a node with nested elements re-encodes and decodes back to the same structure
local nested = {
    name = "root",
    attributes = { id = "1" },
    children = {
        { name = "a", text = "x" },
        { name = "b", children = { { name = "c", text = "y" } } },
    },
}
local back = xml.decode(xml.encode(nested))
assert(back.name == "root" and back.attributes.id == "1", "round-trip name and attribute")
assert(back.children[1].name == "a" and back.children[1].text == "x", "round-trip first child")
assert(back.children[2].name == "b", "round-trip second child name")
assert(back.children[2].children[1].name == "c" and back.children[2].children[1].text == "y", "round-trip grandchild")

-- text special characters survive escaping and unescaping
local esc = xml.decode(xml.encode({ name = "n", text = 'a < b & c > d " e' }))
assert(esc.text == 'a < b & c > d " e', "special characters round-trip in text")

-- attribute special characters survive escaping and unescaping
local attr = xml.decode(xml.encode({ name = "n", attributes = { k = 'a"b<c&d' } }))
assert(attr.attributes.k == 'a"b<c&d', "special characters round-trip in an attribute")

-- pretty true indents with two spaces, an explicit indent sets the width
assert(xml.encode({ name = "a", children = { { name = "b", text = "c" } } }, { pretty = true }):find("\n  <b>", 1, true),
    "pretty uses a two-space indent")
assert(xml.encode({ name = "a", children = { { name = "b", text = "c" } } }, { indent = 4 }):find("\n    <b>", 1, true),
    "indent of four spaces")

-- parse and stringify are aliases of decode and encode
assert(type(xml.parse) == "function" and type(xml.stringify) == "function", "aliases exist")
assert(xml.parse("<a>x</a>").text == "x", "parse alias decodes")
assert(xml.stringify({ name = "a", text = "x" }):find("<a>x</a>", 1, true), "stringify alias encodes")

-- encode requires a table and rejects anything else through pcall
assert(not pcall(xml.encode, "string"), "encode rejects a non-table node")
assert(not pcall(xml.encode, 42), "encode rejects a number node")

-- decode raises on invalid xml and pcall catches it
assert(not pcall(xml.decode, "<unclosed>"), "unclosed tag raises")
assert(not pcall(xml.decode, "not xml at all"), "non-xml input raises")
assert(not pcall(xml.decode, ""), "empty input raises")

-- a utf-8 element name survives encoding instead of being mangled to underscores
local utf8Encoded = xml.encode({ name = "caf\xc3\xa9", text = "x" })
assert(utf8Encoded:find("caf\xc3\xa9", 1, true), "a utf-8 element name should survive encoding")
assert(xml.decode(utf8Encoded).name == "caf\xc3\xa9", "a utf-8 element name should round-trip")

-- xml-illegal control characters are stripped so the emitted document stays well-formed
local cleaned = xml.decode(xml.encode({ name = "a", text = "x\1\2\3y" }))
assert(cleaned.text == "xy", "control characters should be stripped from text")

-- tab, newline and carriage return are legal xml characters and survive encoding
local preserved = xml.decode(xml.encode({ name = "a", text = "a\tb\nc" }))
assert(preserved.text == "a\tb\nc", "tab, newline and carriage return should be preserved")

print("xml features ok")
