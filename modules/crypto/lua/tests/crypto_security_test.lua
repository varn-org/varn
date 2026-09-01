-- crypto security suite covering the lua-reachable abuse cases from docs/security-tests/crypto.md across CWE-20 input validation, CWE-400 resource caps, CWE-193 cap boundaries, CWE-626 nul-aware reads, CWE-327 known vectors, and CWE-330 csprng output
local crypto = require("crypto")

-- CRY-004 / CRY-179 an unknown algorithm produces a clean error, not a crash
assert(not pcall(crypto.digest, "bogus-algo", "data", "hex"), "unknown digest algorithm rejected")

-- CRY-005 / CRY-180 an empty algorithm name is rejected
assert(not pcall(crypto.digest, "", "data", "hex"), "empty algorithm rejected")

-- CRY-011 passing a non-hash algorithm name to hmac is rejected
assert(not pcall(crypto.hmac, "__NOPE__", "key", "data", "hex"), "non-hash hmac algorithm rejected")

-- CRY-026 / CRY-073 an invalid format string is rejected
assert(not pcall(crypto.digest, "SHA256", "data", "b64"), "invalid format rejected")

-- no artificial input-size cap because varn runs trusted local code (like node) so large data hashes fine
assert(#crypto.digest("SHA256", string.rep("a", 8 * 1024 * 1024), "hex") == 64, "large digest input hashes")

-- CRY-038 / CRY-176 a count past the addressable limit is rejected, not wrapped into a bad allocation
assert(not pcall(crypto.randomBytes, 1e18), "out-of-range count rejected")

-- CRY-032 / CRY-174 a negative count is rejected
assert(not pcall(crypto.randomBytes, -1), "negative count rejected")

-- CRY-034 / CRY-175 a non-integer count is rejected
assert(not pcall(crypto.randomBytes, 1.5), "float count rejected")

-- CRY-031 a zero count returns the empty string
assert(#crypto.randomBytes(0) == 0, "zero count returns empty")

-- a large but in-range count is honored with no artificial million-byte cap
assert(#crypto.randomBytes(1024 * 1024) == 1024 * 1024, "large in-range count accepted")

-- CRY-013 an empty hmac key is accepted and stays deterministic
assert(crypto.hmac("SHA256", "", "msg", "hex") == crypto.hmac("SHA256", "", "msg", "hex"), "empty key deterministic")

-- CRY-021 / CRY-198 known-answer vectors confirm a correct, constant implementation
assert(crypto.digest("SHA256", "abc", "hex")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256(abc) vector")
assert(crypto.digest("SHA256", "", "hex")
    == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty vector")

-- CRY-022 the rfc 4231 hmac vector confirms keyed hashing correctness
assert(crypto.hmac("SHA256", string.rep("\x0b", 20), "Hi There", "hex")
    == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "hmac rfc 4231 vector")

-- CRY-014 / CRY-064 / CRY-190 data is read length-aware so a nul byte never truncates it
assert(crypto.digest("SHA256", "a\0b", "hex") ~= crypto.digest("SHA256", "a", "hex"), "nul in data is significant")

-- CRY-015 key bytes after a nul are still used
assert(crypto.hmac("SHA256", "k\0x", "m", "hex") ~= crypto.hmac("SHA256", "k", "m", "hex"), "nul in key is significant")

-- CRY-029 / CRY-035 / CRY-037 csprng output varies and preserves nul bytes verbatim
local a = crypto.randomBytes(64)
local b = crypto.randomBytes(64)
assert(a ~= b, "random draws differ")
assert(#crypto.randomBytes(256) == 256, "random output keeps every byte")

print("crypto security ok")
