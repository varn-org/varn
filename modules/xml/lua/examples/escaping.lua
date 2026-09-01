-- text and attribute specials are escaped and invalid names are sanitized
local xml = require("xml")

-- special characters in text and attributes are escaped on encode
print(xml.encode({ name = "msg", attributes = { note = 'a & b "c"' }, text = "x < y > z" }))

-- an invalid element name is sanitized so it cannot break the document
print(xml.encode({ name = "bad name!", text = "ok" }))

-- escaped values round-trip back to their original form
local back = xml.decode(xml.encode({ name = "n", text = "a < b & c" }))
print(back.text)
