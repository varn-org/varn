-- exercises every documented function, format option, and reachable edge case
local crypto = require("crypto")

-- digest default format is hex and matches the rfc 6234 vector for "abc"
assert(crypto.digest("SHA256", "abc")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256(abc) default hex")
assert(crypto.digest("SHA256", "abc", "hex")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256(abc) explicit hex")

-- the empty-input digest matches the published constant
assert(crypto.digest("SHA256", "", "hex")
    == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty")

-- sha512 produces a 128-character lowercase hex digest
local hex512 = crypto.digest("SHA512", "varn", "hex")
assert(#hex512 == 128, "sha512 hex length")
assert(hex512:match("^[0-9a-f]+$"), "sha512 hex is lowercase")

-- raw format returns the binary digest at the algorithm's native size
assert(#crypto.digest("SHA256", "abc", "raw") == 32, "sha256 raw length")
assert(#crypto.digest("SHA512", "abc", "raw") == 64, "sha512 raw length")

-- raw output is binary-safe and re-encodes to the same hex digest
local raw = crypto.digest("SHA256", "abc", "raw")
local rehex = {}
for i = 1, #raw do
    rehex[i] = string.format("%02x", raw:byte(i))
end
assert(table.concat(rehex) == crypto.digest("SHA256", "abc", "hex"), "raw matches hex")

-- input is length-aware so a nul byte does not truncate the data
assert(crypto.digest("SHA256", "a\0b", "hex") ~= crypto.digest("SHA256", "a", "hex"), "nul-aware data")

-- hmac matches rfc 4231 test case 1 and respects the format options
local key = string.rep("\x0b", 20)
assert(crypto.hmac("SHA256", key, "Hi There", "hex")
    == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "hmac sha256 rfc 4231")
assert(#crypto.hmac("SHA256", key, "Hi There", "raw") == 32, "hmac raw length")
assert(crypto.hmac("SHA256", key, "Hi There") == crypto.hmac("SHA256", key, "Hi There", "hex"), "hmac default hex")

-- an empty hmac key is accepted and stays deterministic across calls
assert(crypto.hmac("SHA256", "", "msg", "hex") == crypto.hmac("SHA256", "", "msg", "hex"), "empty key deterministic")

-- randomBytes returns exactly the requested length, including zero and the documented cap
assert(#crypto.randomBytes(0) == 0, "randomBytes(0)")
assert(#crypto.randomBytes(16) == 16, "randomBytes(16)")
assert(#crypto.randomBytes(1024 * 1024) == 1024 * 1024, "randomBytes at the cap")

-- two draws of the same length differ, confirming a real random source
assert(crypto.randomBytes(32) ~= crypto.randomBytes(32), "randomBytes varies")

-- an unknown algorithm name is rejected with an error rather than a crash
assert(not pcall(crypto.digest, "__NOPE__", "data", "hex"), "unknown digest algorithm rejected")
assert(not pcall(crypto.hmac, "__NOPE__", "key", "data", "hex"), "unknown hmac algorithm rejected")

-- an empty algorithm name is rejected
assert(not pcall(crypto.digest, "", "data", "hex"), "empty algorithm rejected")

-- an invalid format string is rejected
assert(not pcall(crypto.digest, "SHA256", "data", "b64"), "invalid format rejected")

-- a negative count is rejected because it cannot map to a byte length
assert(not pcall(crypto.randomBytes, -1), "negative randomBytes rejected")

-- there is no artificial input-size cap because the caller owns the machine and can hash large data like node
assert(#crypto.digest("SHA256", string.rep("a", 8 * 1024 * 1024), "hex") == 64, "hashes multi-megabyte input")

-- constant-time equals matches identical strings, rejects any difference, length mismatch, and is nul-aware
assert(crypto.equals("secret", "secret") == true, "equals matches identical")
assert(crypto.equals("secret", "secreT") == false, "equals rejects one-byte difference")
assert(crypto.equals("secret", "secre") == false, "equals rejects length mismatch")
assert(crypto.equals("", "") == true, "equals matches empty")
assert(crypto.equals("a\0b", "a\0b") == true, "equals is nul-aware")
assert(crypto.equals("a\0b", "a\0c") == false, "equals compares past a nul")

-- rsaEncryptPublic (oaep) produces a modulus-sized ciphertext and rejects a malformed key
local rsaPublicKey = [[
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAu6x7u0bmx3OlJio/0Xig
iNBbNgNGOW5gj7X3v+CEHSsqyREp4MkM+O9DrkLJp9M2F291SsSlXsamCuY+t7Jp
SUPlf7EpM+h050Wu+HVEoJSsis2QshMt0M9THWKhdZ2m8B2uLRcm16r5V4WFv7U1
4cn7cjUwphkvw2/KzCpZgdHbCNEXilHnOWevUPOJLSdF+u/wBa1qeNlrTr85xnYi
QrpSIe8Chgt9S2SW2y73ZMVhmON225WR8beBlVJT3YKgU8K3Hn6U/TvJyXZogBH1
+CbgohBYTUeQhpYO340wf74SI/d3liFO4NVGjwhb4pourYnXZdBBQytYegLFfNyy
/QIDAQAB
-----END PUBLIC KEY-----
]]
local cipher = crypto.rsaEncryptPublic(rsaPublicKey, "secret payload")
assert(#cipher == 256, "a 2048-bit key yields a 256-byte ciphertext, got " .. #cipher)
-- oaep padding is randomized, so two encryptions of the same input differ
assert(crypto.rsaEncryptPublic(rsaPublicKey, "secret payload") ~= cipher, "oaep encryption is randomized")
assert(not pcall(crypto.rsaEncryptPublic, "not a pem key", "x"), "a malformed public key is rejected")

print("crypto features ok")
