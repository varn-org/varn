-- checks a short string against a known hex fingerprint
local crypto = require("crypto")

local h = crypto.digest("SHA256", "varn", "hex")
assert(#h == 64, "sha256 hex length")
assert(h:match("^[0-9a-f]+$"), "expected lowercase hex")
print("crypto.digest SHA256 hex ok:", h:sub(1, 16), "...")
