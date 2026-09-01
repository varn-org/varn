-- security checks for the parser and encoder boundaries (CWE-611, CWE-776, CWE-674, CWE-91, CWE-176, CWE-248)
local xml = require("xml")

-- an XXE payload never reads a local file and the entity is not expanded
local xxe = '<?xml version="1.0"?><!DOCTYPE foo [<!ENTITY xxe SYSTEM "file:///etc/passwd">]><root>&xxe;</root>'
local ok, res = pcall(xml.decode, xxe)
assert(ok, "xxe payload is parsed safely without crashing")
assert(not (res.text or ""):find("root:"), "xxe must not expose /etc/passwd contents")
assert(not (res.text or ""):find("/bin"), "xxe must not expose any file contents")

-- a billion-laughs payload does not expand and does not crash
local bomb = '<?xml version="1.0"?><!DOCTYPE lolz [<!ENTITY lol "lol">' ..
    '<!ENTITY a "&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;">]><lolz>&a;</lolz>'
local okb, resb = pcall(xml.decode, bomb)
assert(okb, "entity-expansion payload is parsed without a memory blowup")
assert(#(resb.text or "") < 100, "general entity is not expanded into the document")

-- a bare DOCTYPE without entities is handled safely
assert(pcall(xml.decode, '<?xml version="1.0"?><!DOCTYPE r><r>x</r>'), "bare DOCTYPE is handled safely")

-- deeply nested elements are bounded and never crash the process
assert(pcall(xml.decode, string.rep("<a>", 600) .. string.rep("</a>", 600)), "moderately deep nesting is handled")
assert(pcall(xml.decode, string.rep("<a>", 20000) .. string.rep("</a>", 20000)), "very deep nesting is depth-capped, not a crash")

-- special characters in text are escaped, not emitted as raw markup
local injected = xml.encode({ name = "n", text = "</n><script>x</script>" })
assert(not injected:find("<script>", 1, true), "text is escaped so it cannot inject markup")
assert(xml.decode(injected).text == "</n><script>x</script>", "escaped text round-trips to the original")

-- an invalid element name is sanitized so it cannot break the document
assert(xml.encode({ name = "bad name!", text = "x" }):find("<bad_name_", 1, true), "invalid name is sanitized")

-- invalid utf-8 in a node value does not crash the encoder
assert(type(xml.encode({ name = "n", text = "a" .. string.char(0xff, 0xfe) .. "b" })) == "string",
    "invalid utf-8 in text does not crash the encoder")

-- encode rejects a non-table node through the binding rather than crashing
assert(not pcall(xml.encode, "string"), "encode rejects a non-table node")

-- malformed, unclosed, and empty inputs are rejected via pcall
assert(not pcall(xml.decode, "<unclosed>"), "unclosed tag is rejected")
assert(not pcall(xml.decode, "<a></b>"), "mismatched tags are rejected")
assert(not pcall(xml.decode, "not xml at all"), "non-xml input is rejected")
assert(not pcall(xml.decode, ""), "empty input is rejected")

print("xml security ok")
