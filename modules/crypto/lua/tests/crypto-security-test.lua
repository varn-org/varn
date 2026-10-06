-- Covers the Lua-reachable abuse cases from `docs/security-tests/crypto.md` across CWE-20 input validation, CWE-400 resource caps, CWE-193 cap boundaries, CWE-626 NUL-aware reads, CWE-327 known vectors, and CWE-330 CSPRNG output.
local crypto = require("crypto")

-- CRY-004 / CRY-179: An unknown algorithm produces a clean error, not a crash.
assert(not pcall(crypto.digest, "bogus-algo", "data", "hex"), "Unknown digest algorithm rejected")

-- CRY-005 / CRY-180: An empty algorithm name is rejected.
assert(not pcall(crypto.digest, "", "data", "hex"), "Empty algorithm rejected")

-- CRY-011: Passing a non-hash algorithm name to HMAC is rejected.
assert(not pcall(crypto.hmac, "__NOPE__", "key", "data", "hex"), "Non-hash HMAC algorithm rejected")

-- CRY-026 / CRY-073: An invalid format string is rejected.
assert(not pcall(crypto.digest, "SHA256", "data", "b64"), "Invalid format rejected")

-- There is no artificial input-size cap because Varn runs trusted local code (like Node), so large data hashes fine.
assert(#crypto.digest("SHA256", string.rep("a", 8 * 1024 * 1024), "hex") == 64, "Large digest input hashes")

-- CRY-038 / CRY-176: A count past the addressable limit is rejected, not wrapped into a bad allocation.
assert(not pcall(crypto.randomBytes, 1e18), "Out-of-range count rejected")

-- CRY-032 / CRY-174: A negative count is rejected.
assert(not pcall(crypto.randomBytes, -1), "Negative count rejected")

-- CRY-034 / CRY-175: A non-integer count is rejected.
assert(not pcall(crypto.randomBytes, 1.5), "Float count rejected")

-- CRY-031: A zero count returns the empty string.
assert(#crypto.randomBytes(0) == 0, "Zero count returns empty")

-- A large but in-range count is honored with no artificial million-byte cap.
assert(#crypto.randomBytes(1024 * 1024) == 1024 * 1024, "Large in-range count accepted")

-- CRY-013: An empty HMAC key is accepted and stays deterministic.
assert(crypto.hmac("SHA256", "", "msg", "hex") == crypto.hmac("SHA256", "", "msg", "hex"), "Empty key is deterministic")

-- CRY-021 / CRY-198: Known-answer vectors confirm a correct, constant implementation.
assert(crypto.digest("SHA256", "abc", "hex")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 'SHA-256 of "abc" vector')
assert(crypto.digest("SHA256", "", "hex")
    == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 empty vector")

-- CRY-022: The RFC 4231 HMAC vector confirms keyed hashing correctness.
assert(crypto.hmac("SHA256", string.rep("\x0b", 20), "Hi There", "hex")
    == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "HMAC RFC 4231 vector")

-- CRY-014 / CRY-064 / CRY-190: Data is read length-aware so a NUL byte never truncates it.
assert(crypto.digest("SHA256", "a\0b", "hex") ~= crypto.digest("SHA256", "a", "hex"), "NUL in data is significant")

-- CRY-015: Key bytes after a NUL are still used.
assert(crypto.hmac("SHA256", "k\0x", "m", "hex") ~= crypto.hmac("SHA256", "k", "m", "hex"), "NUL in key is significant")

-- CRY-029 / CRY-035 / CRY-037: CSPRNG output varies and preserves NUL bytes verbatim.
local a = crypto.randomBytes(64)
local b = crypto.randomBytes(64)
assert(a ~= b, "Random draws differ")
assert(#crypto.randomBytes(256) == 256, "Random output keeps every byte")

print("The \"crypto\" security tests passed.")
