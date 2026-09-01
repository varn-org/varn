-- parse and stringify are aliases of decode and encode
local xml = require("xml")

local node = xml.parse('<user id="1"><name>Lua</name></user>')
print(node.name, node.attributes.id, node.children[1].text)

print(xml.stringify({ name = "ok", text = "done" }))
