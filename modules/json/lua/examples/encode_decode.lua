-- encodes a lua value to text and decodes it back
local json = require("json")

local text = json.encode({ name = "varn", version = "1.0", tags = { "fast", "small" } })
print(text)

local value = json.decode(text)
print(value.name, value.version, value.tags[1], value.tags[2])
