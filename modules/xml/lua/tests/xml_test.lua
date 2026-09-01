-- standalone module node-model round-trip and security checks
local xml = require("xml")

-- decode exposes name, attributes, ordered children, and text
local node = xml.decode('<root id="1" lang="en"><item>hi</item><item>bye</item></root>')
assert(node.name == "root", "root name")
assert(node.attributes.id == "1" and node.attributes.lang == "en", "attributes")
assert(#node.children == 2, "two children")
assert(node.children[1].text == "hi" and node.children[2].text == "bye", "child text")

-- encode then decode round-trips structure
local doc = xml.encode({ name = "doc", attributes = { v = "1" }, children = { { name = "x", text = "hello" } } })
assert(doc:find("<doc", 1, true) and doc:find('v="1"', 1, true), "encode attributes")
local back = xml.decode(doc)
assert(back.name == "doc" and back.children[1].text == "hello", "encode/decode round-trip")

-- text special characters survive escaping and unescaping
local esc = xml.decode(xml.encode({ name = "n", text = 'a < b & c > d " e' }))
assert(esc.text == 'a < b & c > d " e', "text escape round-trip: " .. tostring(esc.text))

-- element names are sanitized so invalid characters cannot break the document
assert(xml.encode({ name = "bad name!", text = "x" }):find("<bad_name_", 1, true), "name sanitized")

-- pretty printing
assert(xml.encode({ name = "a", children = { { name = "b", text = "c" } } }, { pretty = true }):find("\n"), "pretty newline")

-- XXE must never read local files
local xxe = '<?xml version="1.0"?><!DOCTYPE foo [<!ENTITY xxe SYSTEM "file:///etc/passwd">]><root>&xxe;</root>'
local ok, res = pcall(xml.decode, xxe)
if ok then
    assert(not (res.text or ""):find("root:"), "XXE must not expose /etc/passwd")
end

-- billion laughs must not expand (no memory blowup or crash)
pcall(xml.decode,
    '<?xml version="1.0"?><!DOCTYPE lolz [<!ENTITY lol "lol"><!ENTITY a "&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;">]><lolz>&a;</lolz>')

-- deep nesting must not overflow the stack (depth-capped)
assert(pcall(xml.decode, string.rep("<a>", 600) .. string.rep("</a>", 600)), "deep nesting bounded")

-- malformed, non-xml, and empty input is rejected
assert(not pcall(xml.decode, "<unclosed>"), "unclosed rejected")
assert(not pcall(xml.decode, "not xml at all"), "non-xml rejected")
assert(not pcall(xml.decode, ""), "empty rejected")

-- encode requires a node table
assert(not pcall(xml.encode, "string"), "encode rejects non-table")

print("xml ok")
