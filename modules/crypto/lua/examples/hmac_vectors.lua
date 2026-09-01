-- verifies HMAC-SHA256 against RFC 4231 test case 1
local crypto = require("crypto")

local key = string.rep("\x0b", 20)
local mac = crypto.hmac("SHA256", key, "Hi There", "hex")
assert(mac == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "HMAC-SHA256 RFC 4231 case 1")

print("crypto.hmac RFC 4231 vector ok")
