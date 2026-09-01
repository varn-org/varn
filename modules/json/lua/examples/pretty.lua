-- pretty-prints with a default or explicit indent
local json = require("json")

print(json.encode({ user = { id = 1, roles = { "admin", "user" } } }, { pretty = true }))
print(json.encode({ a = 1, b = 2 }, { indent = 4 }))
