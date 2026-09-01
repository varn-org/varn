-- json maps onto lua tables in both directions
local json = require("json")

local encoded = json.encode({
    name = "varn",
    tags = { "lua", "c++" },
    active = true,
    count = 3,
})
print("encoded:", encoded)

local decoded = json.decode('{"a":1,"b":[10,20],"c":{"d":true}}')
print("a:", decoded.a)
print("b[2]:", decoded.b[2])
print("c.d:", decoded.c.d)
