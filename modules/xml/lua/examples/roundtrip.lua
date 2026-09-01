-- parse a document, walk its nodes, and re-encode it
local xml = require("xml")

local original = '<note priority="high"><to>Lua</to><from>C++</from><body>hello &amp; bye</body></note>'

local node = xml.decode(original)
print("root:", node.name, "priority:", node.attributes.priority)
for _, child in ipairs(node.children) do
    print(child.name, "=", child.text)
end

print(xml.encode(node))
