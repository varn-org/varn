-- shows randomBytes drawing a large requested size and rejecting a negative count
local crypto = require("crypto")

local bytes = crypto.randomBytes(1024 * 1024)
assert(#bytes == 1024 * 1024, "draw the requested size")

local ok = pcall(crypto.randomBytes, -1)
assert(not ok, "a negative count is rejected")

print("crypto.randomBytes ok: len=", #bytes, "negative rejected")
