-- Security checks for the parser and encoder boundaries (CWE-611, CWE-776, CWE-674, CWE-91, CWE-176, CWE-248).
local xml = require("xml")

-- An XXE payload never reads a local file and the entity is not expanded.
local xxe = '<?xml version="1.0"?><!DOCTYPE foo [<!ENTITY xxe SYSTEM "file:///etc/passwd">]><root>&xxe;</root>'
local ok, res = pcall(xml.decode, xxe)
assert(ok, "An XXE payload is parsed safely without crashing")
assert(not (res.text or ""):find("root:"), 'An XXE payload must not expose the "/etc/passwd" contents')
assert(not (res.text or ""):find("/bin"), "An XXE payload must not expose any file contents")

-- A billion-laughs payload does not expand and does not crash.
local bomb = '<?xml version="1.0"?><!DOCTYPE lolz [<!ENTITY lol "lol">' ..
    '<!ENTITY a "&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;">]><lolz>&a;</lolz>'
local okb, resb = pcall(xml.decode, bomb)
assert(okb, "An entity-expansion payload is parsed without a memory blowup")
assert(#(resb.text or "") < 100, "A general entity is not expanded into the document")

-- A bare DOCTYPE without entities is handled safely.
assert(pcall(xml.decode, '<?xml version="1.0"?><!DOCTYPE r><r>x</r>'), "A bare DOCTYPE is handled safely")

-- Deeply nested elements are bounded and never crash the process, and nesting past the bound raises rather than dropping the deeper elements.
assert(pcall(xml.decode, string.rep("<a>", 200) .. string.rep("</a>", 200)), "Nesting within the bound is handled")
for _, depth in ipairs({ 600, 20000 }) do
    local okDeep, errDeep = pcall(xml.decode, string.rep("<a>", depth) .. string.rep("</a>", depth))
    assert(not okDeep, "Nesting past the bound should raise")
    assert(tostring(errDeep):find("The XML document nests deeper than 256 elements.", 1, true), "The error should name the bound, got " .. tostring(errDeep))
end

-- Special characters in text are escaped, not emitted as raw markup.
local injected = xml.encode({ name = "n", text = "</n><script>x</script>" })
assert(not injected:find("<script>", 1, true), "Text is escaped so it cannot inject markup")
assert(xml.decode(injected).text == "</n><script>x</script>", "Escaped text round-trips to the original")

-- An invalid element or attribute name is refused so it cannot break the document, and so is an empty one.
local okName, nameErr = pcall(xml.encode, { name = "bad name!", text = "x" })
assert(not okName and tostring(nameErr):find('The element name "bad name!" is not a valid XML name.', 1, true), "An invalid name should be refused, got " .. tostring(nameErr))
local okAttribute, attributeErr = pcall(xml.encode, { name = "n", attributes = { ['x"y'] = "1" } })
assert(not okAttribute and tostring(attributeErr):find('of the element "n" is not a valid XML name.', 1, true), "An invalid attribute name should be refused, got " .. tostring(attributeErr))
assert(not pcall(xml.encode, { name = "" }), "An empty name should be refused")
assert(not pcall(xml.encode, { name = "a\0b" }), "A name with a null byte should be refused rather than cut short")

-- Invalid UTF-8 in a node value is refused rather than written into a document no parser reads.
local okUtf8, utf8Err = pcall(xml.encode, { name = "n", text = "a" .. string.char(0xff, 0xfe) .. "b" })
assert(not okUtf8 and tostring(utf8Err):find('The text of the element "n" is not valid UTF-8.', 1, true), "Invalid UTF-8 should be refused, got " .. tostring(utf8Err))
local okAttributeText, attributeTextErr = pcall(xml.encode, { name = "n", attributes = { id = "\xed\xa0\x80" } })
assert(not okAttributeText and tostring(attributeTextErr):find('The attribute "id" of the element "n" is not valid UTF-8.', 1, true), "An encoded surrogate should be refused, got " .. tostring(attributeTextErr))

-- The `encode` call rejects a non-table node through the binding rather than crashing.
assert(not pcall(xml.encode, "string"), 'The call "encode" rejects a non-table node')

-- Malformed, unclosed, and empty inputs are rejected via `pcall`.
assert(not pcall(xml.decode, "<unclosed>"), "An unclosed tag is rejected")
assert(not pcall(xml.decode, "<a></b>"), "Mismatched tags are rejected")
assert(not pcall(xml.decode, "not xml at all"), "A non-XML input is rejected")
assert(not pcall(xml.decode, ""), "An empty input is rejected")

print("The \"xml\" security tests passed.")
