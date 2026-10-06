-- Exercises the codecs, UUIDs, password hashing, AES-GCM, and key derivation helpers.
local crypto = require("crypto")

-- Turns a binary string into lowercase hex for comparing against known vectors.
local function tohex(s)
    local parts = {}
    for i = 1, #s do
        parts[i] = string.format("%02x", s:byte(i))
    end
    return table.concat(parts)
end

-- Base64 round-trips arbitrary binary including embedded NUL bytes.
local binary = "\0\1\2\3\255\254\0abc\0"
assert(crypto.base64Decode(crypto.base64Encode(binary)) == binary, "Base64 binary round-trip")
assert(crypto.base64Encode("") == "", "Base64 empty encodes empty")
assert(crypto.base64Decode("") == "", "Base64 empty decodes empty")

-- Base64 matches known padded vectors.
assert(crypto.base64Encode("f") == "Zg==", 'Base64 of "f"')
assert(crypto.base64Encode("fo") == "Zm8=", 'Base64 of "fo"')
assert(crypto.base64Encode("foo") == "Zm9v", 'Base64 of "foo"')
assert(crypto.base64Encode("foobar") == "Zm9vYmFy", 'Base64 of "foobar"')
assert(crypto.base64Decode("Zm9vYmFy") == "foobar", 'Base64 decode of "foobar"')

-- Base64url uses the URL-safe alphabet and emits no padding.
local urlSrc = "\251\255\190"
local urlEnc = crypto.base64UrlEncode(urlSrc)
assert(urlEnc:find("=", 1, true) == nil, "Base64url has no padding")
assert(urlEnc:find("[+/]") == nil, 'Base64url avoids "+" and "/"')
assert(crypto.base64UrlDecode(urlEnc) == urlSrc, "Base64url round-trip")

-- Invalid Base64 input is rejected loudly.
assert(not pcall(crypto.base64Decode, "Zm9v*"), "Base64 rejects an invalid character")

-- Hex round-trips binary including NUL and matches a known vector.
assert(crypto.hexEncode("a\0b") == "610062", "Hex encode is NUL-aware")
assert(crypto.hexDecode("610062") == "a\0b", "Hex decode is NUL-aware")
assert(crypto.hexDecode(crypto.hexEncode(binary)) == binary, "Hex binary round-trip")
assert(not pcall(crypto.hexDecode, "abc"), "Hex rejects an odd length")
assert(not pcall(crypto.hexDecode, "zz"), "Hex rejects an invalid character")

-- A UUID v4 has the canonical shape with the version and variant nibbles fixed.
local u = crypto.uuidV4()
assert(#u == 36, "UUID length")
assert(u:match("^%x%x%x%x%x%x%x%x%-%x%x%x%x%-4%x%x%x%-[89ab]%x%x%x%-%x%x%x%x%x%x%x%x%x%x%x%x$"), "UUID v4 shape")
assert(crypto.uuidV4() ~= crypto.uuidV4(), "UUID is unique")

-- A UUID v7 has the canonical shape with version 7 and the RFC 4122 variant.
local u7 = crypto.uuidV7()
assert(u7:match("^%x%x%x%x%x%x%x%x%-%x%x%x%x%-7%x%x%x%-[89ab]%x%x%x%-%x%x%x%x%x%x%x%x%x%x%x%x$"), "UUID v7 shape")
assert(crypto.uuidV7() ~= crypto.uuidV7(), "UUID v7 is unique")

-- Password hashing is self-describing, verifies the right password, and rejects the wrong one.
local hash = crypto.hashPassword("correct horse battery staple")
assert(hash:match("^scrypt%$"), "Password hash is self-describing")
assert(crypto.verifyPassword("correct horse battery staple", hash) == true, "Verify accepts the correct password")
assert(crypto.verifyPassword("wrong password", hash) == false, "Verify rejects a wrong password")
assert(crypto.verifyPassword("correct horse battery staple", "garbage") == false, "Verify rejects a malformed hash")

-- Two hashes of the same password differ because the salt is random.
assert(crypto.hashPassword("same") ~= crypto.hashPassword("same"), "Password salt is random")

-- The cost parameters of a stored hash decide how much memory verifying allocates, so an out-of-range set is refused instead of being handed to scrypt.
local saltField, hashField = hash:match("^scrypt%$[^$]+%$([^$]+)%$(.+)$")
assert(saltField and hashField, "The hash fields should be readable")

local function crafted(params)
    return "scrypt$" .. params .. "$" .. saltField .. "$" .. hashField
end

local started = os.clock()
assert(crypto.verifyPassword("correct horse battery staple", crafted("1073741824,8,1")) == false, "An oversized cost is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,4294967296,1")) == false, "A block size past the cap is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,8,4294967296")) == false, "A parallelism past the cap is refused")
-- A 32-bit narrowing of these fields would turn each of these into the accepted parameter set, so they must be rejected on their full width.
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,4294967304,1")) == false, "A block size that narrows to a valid one is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,8,4294967297")) == false, "A parallelism that narrows to a valid one is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("0,8,1")) == false, "A degenerate cost is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,0,1")) == false, "A zero block size is refused")
assert(os.clock() - started < 1.0, "Refusing bad parameters must not do the work first")

-- The parameters this build writes stay inside the accepted range.
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,8,1")) == true, "The shipped cost parameters remain valid")

-- AES-256-GCM encrypt then decrypt recovers the plaintext.
local key = crypto.randomBytes(32)
local message = "secret message with a \0 nul byte"
local blob = crypto.encrypt(key, message)
assert(blob ~= message, "Ciphertext differs from plaintext")
assert(crypto.decrypt(key, blob) == message, "AES-GCM round-trip")

-- The empty plaintext round-trips through AES-GCM.
assert(crypto.decrypt(key, crypto.encrypt(key, "")) == "", "AES-GCM empty round-trip")

-- A tampered blob fails authentication.
local tampered = blob:sub(1, #blob - 1) .. string.char((blob:byte(#blob) + 1) % 256)
assert(not pcall(crypto.decrypt, key, tampered), "AES-GCM detects tampering")

-- A wrong key fails authentication.
assert(not pcall(crypto.decrypt, crypto.randomBytes(32), blob), "AES-GCM rejects a wrong key")

-- A key of the wrong size is rejected.
assert(not pcall(crypto.encrypt, "short", message), "AES-GCM rejects a short key")

-- PBKDF2-HMAC-SHA256 matches a known vector (password, salt, 1 iteration, 32 bytes).
local dk = crypto.pbkdf2("password", "salt", 1, 32, "SHA256")
assert(tohex(dk) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b", "PBKDF2 SHA-256 vector")
assert(#crypto.pbkdf2("password", "salt", 1000, 16, "SHA256") == 16, "PBKDF2 honors the key length")

-- HKDF-SHA256 matches RFC 5869 test case 1.
local ikm = string.rep("\x0b", 22)
local hsalt = ""
for i = 0, 12 do
    hsalt = hsalt .. string.char(i)
end
local info = ""
for i = 0xf0, 0xf9 do
    info = info .. string.char(i)
end
local okm = crypto.hkdf(ikm, hsalt, info, 42, "SHA256")
assert(tohex(okm) == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865", "HKDF SHA-256 RFC 5869 case 1")

print("The \"crypto\" extra tests passed.")
