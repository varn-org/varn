-- indent sets the indentation width in spaces
local xml = require("xml")

local node = { name = "root", children = { { name = "item", text = "value" } } }
print(xml.encode(node, { indent = 4 }))
