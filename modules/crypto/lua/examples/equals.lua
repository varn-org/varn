-- constant-time comparison for verifying a mac or token without leaking its bytes through timing
local crypto = require("crypto")

local mac = crypto.hmac("SHA256", "server-secret", "session-42")

-- the caller presents a token verified in constant time, never with the == operator
local presented = crypto.hmac("SHA256", "server-secret", "session-42")
assert(crypto.equals(mac, presented), "a matching token verifies")
assert(not crypto.equals(mac, "tampered"), "a wrong token is rejected")

print("crypto.equals ok")
