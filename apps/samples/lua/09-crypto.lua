-- the full crypto surface is available on a native build
local crypto = require("crypto")

print("sha256:", crypto.digest("SHA256", "varn"):sub(1, 32) .. "...")
print("hmac:", crypto.hmac("SHA256", "key", "varn"):sub(1, 32) .. "...")
print("base64:", crypto.base64Encode("varn everywhere"))
print("hex:", crypto.hexEncode("varn"))
print("uuid:", crypto.uuidV7())

local hash = crypto.hashPassword("correct horse")
print("verified:", crypto.verifyPassword("correct horse", hash))
print("rejected:", crypto.verifyPassword("wrong", hash))
