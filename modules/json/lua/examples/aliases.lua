-- shows stringify and parse as aliases of encode and decode
local json = require("json")

local text = json.stringify({ id = 1, active = true })
print(text)

local value = json.parse(text)
print(value.id, value.active)
