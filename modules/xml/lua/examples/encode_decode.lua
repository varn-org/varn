-- build a document from the node model and parse it back
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
