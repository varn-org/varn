-- Xml encodes and decodes with the same table mapping json uses.
local xml = require("xml")

local document = xml.encode({
    name = "project",
    children = {
        { name = "title", text = "Varn" },
        { name = "language", text = "Lua" },
    },
})
print(document)

local parsed = xml.decode("<root><item id='1'>first</item><item id='2'>second</item></root>")
for _, child in ipairs(parsed.children) do
    print(child.attributes.id, child.text)
end
