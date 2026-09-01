-- checks the raw digest length for a fixed input string
local crypto = require("crypto")

local raw = crypto.digest("SHA256", "raw-test", "raw")
assert(#raw == 32, "raw sha256 length")
print("crypto.digest raw ok, len=", #raw)
