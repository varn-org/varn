-- The encoder refuses what XML cannot carry and names it, and nesting past the bound raises on both sides instead of losing elements.
local xml = require("xml")

local function failsWith(fragment, fn, ...)
    local ok, err = pcall(fn, ...)
    assert(not ok, "The call should raise an error containing: " .. fragment)
    assert(tostring(err):find(fragment, 1, true), "The error should contain '" .. fragment .. "', got " .. tostring(err))
end

-- A value that is not text is refused rather than written as an empty or invented stand-in.
failsWith('The attribute "id" of the element "item" is a table, which XML cannot carry.', xml.encode, { name = "item", attributes = { id = {} } })
failsWith('The text of the element "item" is a function, which XML cannot carry.', xml.encode, { name = "item", text = print })
failsWith("An attribute has a key of type boolean, which XML cannot name.", xml.encode, { name = "item", attributes = { [true] = "x" } })
failsWith('Each node needs a "name" that is a string.', xml.encode, { text = "orphan" })
failsWith('Each node needs a "name" that is a string.', xml.encode, { name = 7 })
failsWith("An attribute has a key of type number, which XML cannot name.", xml.encode, { name = "item", attributes = { "x" } })
failsWith('The element name "7up" is not a valid XML name.', xml.encode, { name = "7up" })
failsWith('The child 2 of the element "list" is a string, not a node table.', xml.encode, { name = "list", children = { { name = "a" }, "b" } })

-- Numbers and booleans are written as their text.
local encoded = xml.encode({ name = "item", attributes = { count = 3, ok = true }, text = 1.5 })
local back = xml.decode(encoded)
assert(back.attributes.count == "3" and back.attributes.ok == "true" and back.text == "1.5", "Numbers and booleans should encode as text: " .. encoded)

-- A node tree deeper than the bound raises a sentence that names it.
local deep = { name = "a" }
local cursor = deep
for _ = 1, 300 do
    local child = { name = "a" }
    cursor.children = { child }
    cursor = child
end

failsWith("The node tree nests deeper than 256 elements.", xml.encode, deep)

-- The width of the indentation is a whole number of spaces within its range.
failsWith('The option "indent" must be a whole number of spaces from 0 to 16.', xml.encode, { name = "a" }, { indent = 40 })
failsWith('The option "indent" must be a whole number of spaces from 0 to 16.', xml.encode, { name = "a" }, { indent = 1.5 })
assert(xml.encode({ name = "a", children = { { name = "b" } } }, { indent = 4.0 }):find("\n    <b", 1, true), "An indent given as a float with a whole value is accepted")

print("The \"xml\" error tests passed.")
