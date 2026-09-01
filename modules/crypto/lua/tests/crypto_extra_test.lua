-- exercises the codecs, uuids, password hashing, aes-gcm, and key derivation helpers
local crypto = require("crypto")

-- helper to turn a binary string into lowercase hex for comparing against known vectors
local function tohex(s)
    local parts = {}
    for i = 1, #s do
        parts[i] = string.format("%02x", s:byte(i))
    end
    return table.concat(parts)
end

-- base64 round-trips arbitrary binary including embedded nul bytes
local binary = "\0\1\2\3\255\254\0abc\0"
assert(crypto.base64Decode(crypto.base64Encode(binary)) == binary, "base64 binary round-trip")
assert(crypto.base64Encode("") == "", "base64 empty encodes empty")
assert(crypto.base64Decode("") == "", "base64 empty decodes empty")

-- base64 matches known padded vectors
assert(crypto.base64Encode("f") == "Zg==", "base64 f")
assert(crypto.base64Encode("fo") == "Zm8=", "base64 fo")
assert(crypto.base64Encode("foo") == "Zm9v", "base64 foo")
assert(crypto.base64Encode("foobar") == "Zm9vYmFy", "base64 foobar")
assert(crypto.base64Decode("Zm9vYmFy") == "foobar", "base64 decode foobar")

-- base64url uses the url-safe alphabet and emits no padding
local urlSrc = "\251\255\190"
local urlEnc = crypto.base64UrlEncode(urlSrc)
assert(urlEnc:find("=", 1, true) == nil, "base64url has no padding")
assert(urlEnc:find("[+/]") == nil, "base64url avoids + and /")
assert(crypto.base64UrlDecode(urlEnc) == urlSrc, "base64url round-trip")

-- invalid base64 input is rejected loudly
assert(not pcall(crypto.base64Decode, "Zm9v*"), "base64 rejects invalid character")

-- hex round-trips binary including nul and matches a known vector
assert(crypto.hexEncode("a\0b") == "610062", "hex encode nul-aware")
assert(crypto.hexDecode("610062") == "a\0b", "hex decode nul-aware")
assert(crypto.hexDecode(crypto.hexEncode(binary)) == binary, "hex binary round-trip")
assert(not pcall(crypto.hexDecode, "abc"), "hex rejects odd length")
assert(not pcall(crypto.hexDecode, "zz"), "hex rejects invalid character")

-- uuid v4 has the canonical shape with the version and variant nibbles fixed
local u = crypto.uuidV4()
assert(#u == 36, "uuid length")
assert(u:match("^%x%x%x%x%x%x%x%x%-%x%x%x%x%-4%x%x%x%-[89ab]%x%x%x%-%x%x%x%x%x%x%x%x%x%x%x%x$"), "uuid v4 shape")
assert(crypto.uuidV4() ~= crypto.uuidV4(), "uuid is unique")

-- uuid v7 has the canonical shape with version 7 and the rfc 4122 variant
local u7 = crypto.uuidV7()
assert(u7:match("^%x%x%x%x%x%x%x%x%-%x%x%x%x%-7%x%x%x%-[89ab]%x%x%x%-%x%x%x%x%x%x%x%x%x%x%x%x$"), "uuid v7 shape")
assert(crypto.uuidV7() ~= crypto.uuidV7(), "uuid v7 is unique")

-- password hashing is self-describing, verifies the right password, and rejects the wrong one
local hash = crypto.hashPassword("correct horse battery staple")
assert(hash:match("^scrypt%$"), "password hash is self-describing")
assert(crypto.verifyPassword("correct horse battery staple", hash) == true, "verify accepts correct password")
assert(crypto.verifyPassword("wrong password", hash) == false, "verify rejects wrong password")
assert(crypto.verifyPassword("correct horse battery staple", "garbage") == false, "verify rejects malformed hash")

-- two hashes of the same password differ because the salt is random
assert(crypto.hashPassword("same") ~= crypto.hashPassword("same"), "password salt is random")

-- the cost parameters of a stored hash decide how much memory verifying allocates, so an out-of-range set is refused instead of being handed to scrypt
local saltField, hashField = hash:match("^scrypt%$[^$]+%$([^$]+)%$(.+)$")
assert(saltField and hashField, "the hash fields should be readable")

local function crafted(params)
    return "scrypt$" .. params .. "$" .. saltField .. "$" .. hashField
end

local started = os.clock()
assert(crypto.verifyPassword("correct horse battery staple", crafted("1073741824,8,1")) == false, "an oversized cost is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,4294967296,1")) == false, "a block size past the cap is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,8,4294967296")) == false, "a parallelism past the cap is refused")
-- a 32-bit narrowing of these fields would turn each of these into the accepted parameter set, so they must be rejected on their full width
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,4294967304,1")) == false, "a block size that narrows to a valid one is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,8,4294967297")) == false, "a parallelism that narrows to a valid one is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("0,8,1")) == false, "a degenerate cost is refused")
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,0,1")) == false, "a zero block size is refused")
assert(os.clock() - started < 1.0, "refusing bad parameters must not do the work first")

-- the parameters this build writes stay inside the accepted range
assert(crypto.verifyPassword("correct horse battery staple", crafted("32768,8,1")) == true, "the shipped cost parameters remain valid")

-- aes-256-gcm encrypt then decrypt recovers the plaintext
local key = crypto.randomBytes(32)
local message = "secret message with a \0 nul byte"
local blob = crypto.encrypt(key, message)
assert(blob ~= message, "ciphertext differs from plaintext")
assert(crypto.decrypt(key, blob) == message, "aes-gcm round-trip")

-- the empty plaintext round-trips through aes-gcm
assert(crypto.decrypt(key, crypto.encrypt(key, "")) == "", "aes-gcm empty round-trip")

-- a tampered blob fails authentication
local tampered = blob:sub(1, #blob - 1) .. string.char((blob:byte(#blob) + 1) % 256)
assert(not pcall(crypto.decrypt, key, tampered), "aes-gcm detects tampering")

-- a wrong key fails authentication
assert(not pcall(crypto.decrypt, crypto.randomBytes(32), blob), "aes-gcm rejects wrong key")

-- a key of the wrong size is rejected
assert(not pcall(crypto.encrypt, "short", message), "aes-gcm rejects short key")

-- pbkdf2-hmac-sha256 matches a known vector (password, salt, 1 iteration, 32 bytes)
local dk = crypto.pbkdf2("password", "salt", 1, 32, "SHA256")
assert(tohex(dk) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b", "pbkdf2 sha256 vector")
assert(#crypto.pbkdf2("password", "salt", 1000, 16, "SHA256") == 16, "pbkdf2 honors key length")

-- hkdf-sha256 matches rfc 5869 test case 1
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
assert(tohex(okm) == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865", "hkdf sha256 rfc 5869 case 1")

print("crypto extra ok")
