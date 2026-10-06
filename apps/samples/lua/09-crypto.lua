-- The full crypto surface is available on a native build, and the password hash answers a promise since it runs on the task pool.
local async = require("async")
local crypto = require("crypto")

print("sha256:", crypto.digest("SHA256", "varn"):sub(1, 32) .. "...")
print("hmac:", crypto.hmac("SHA256", "key", "varn"):sub(1, 32) .. "...")
print("base64:", crypto.base64Encode("varn everywhere"))
print("hex:", crypto.hexEncode("varn"))
print("uuid:", crypto.uuidV7())

async.run(function()
    local hash = crypto.hashPassword("correct horse"):await()
    print("verified:", crypto.verifyPassword("correct horse", hash):await())
    print("rejected:", crypto.verifyPassword("wrong", hash):await())
end)
