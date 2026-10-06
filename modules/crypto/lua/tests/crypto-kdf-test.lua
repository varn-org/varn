-- Covers KDF validation and the non-SHA-256 variant that the main crypto tests do not exercise.
local async = require("async")
local crypto = require("crypto")

-- PBKDF2 and HKDF reject a non-positive iteration count or key length.
assert(not pcall(function() return crypto.pbkdf2("pw", "salt", 0, 32, "SHA256") end), "PBKDF2 with 0 iterations should error")
assert(not pcall(function() return crypto.pbkdf2("pw", "salt", 1000, 0, "SHA256") end), "PBKDF2 with a 0 key length should error")
assert(not pcall(function() return crypto.hkdf("ikm", "salt", "info", 0, "SHA256") end), "HKDF with a 0 key length should error")

-- The SHA-512 variant honors the requested length and derives a different key than SHA-256.
async.run(function()
    local a = crypto.pbkdf2("pw", "salt", 1000, 32, "SHA256"):await()
    local b = crypto.pbkdf2("pw", "salt", 1000, 32, "SHA512"):await()
    assert(#a == 32 and #b == 32, "PBKDF2 honors the key length for both algorithms")
    assert(a ~= b, "PBKDF2 SHA-256 and SHA-512 derive different keys")
    assert(#crypto.hkdf("ikm", "salt", "info", 48, "SHA512") == 48, "HKDF honors the key length for SHA-512")

    -- An unknown algorithm rejects the promise rather than raising, since it is found on the task pool.
    local key, err = crypto.pbkdf2("pw", "salt", 1, 32, "NOT-A-DIGEST"):await()
    assert(key == nil and err:find("not known", 1, true), "PBKDF2 with an unknown algorithm should reject")

    print("The \"crypto\" KDF tests passed.")
end)
