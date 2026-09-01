-- asserts only on the length fields returned for random byte requests
local crypto = require("crypto")

local b16 = crypto.randomBytes(16)
assert(#b16 == 16, "length")
local b32 = crypto.randomBytes(32)
assert(#b32 == 32, "length")
print("crypto.randomBytes ok")
