-- kdf validation and the non-sha256 variant that the main crypto tests do not exercise
local crypto = require("crypto")

-- pbkdf2 and hkdf reject a non-positive iteration count or key length
assert(not pcall(function() return crypto.pbkdf2("pw", "salt", 0, 32, "SHA256") end), "pbkdf2 with 0 iterations should error")
assert(not pcall(function() return crypto.pbkdf2("pw", "salt", 1000, 0, "SHA256") end), "pbkdf2 with a 0 key length should error")
assert(not pcall(function() return crypto.hkdf("ikm", "salt", "info", 0, "SHA256") end), "hkdf with a 0 key length should error")

-- the sha512 variant honors the requested length and derives a different key than sha256
local a = crypto.pbkdf2("pw", "salt", 1000, 32, "SHA256")
local b = crypto.pbkdf2("pw", "salt", 1000, 32, "SHA512")
assert(#a == 32 and #b == 32, "pbkdf2 honors the key length for both algorithms")
assert(a ~= b, "pbkdf2 sha256 and sha512 derive different keys")
assert(#crypto.hkdf("ikm", "salt", "info", 48, "SHA512") == 48, "hkdf honors the key length for sha512")

print("crypto kdf ok")
