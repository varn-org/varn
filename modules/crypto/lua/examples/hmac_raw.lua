-- produces a keyed hmac in raw binary form and checks its length
local crypto = require("crypto")

local raw = crypto.hmac("SHA256", "secret-key", "message", "raw")
assert(#raw == 32, "raw hmac sha256 length")

print("crypto.hmac raw ok, len=", #raw)
