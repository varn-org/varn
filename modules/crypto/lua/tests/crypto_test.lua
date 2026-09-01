-- known digest/hmac vectors, digest sizes, and random byte lengths
local crypto = require("crypto")

-- sha256("abc") rfc 6234 test vector
assert(crypto.digest("SHA256", "abc", "hex")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256(abc)")
assert(#crypto.digest("SHA512", "varn", "hex") == 128, "sha512 hex length")

-- hmac-sha256 rfc 4231 case 1
local mac = crypto.hmac("SHA256", string.rep("\x0b", 20), "Hi There", "hex")
assert(mac == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "hmac sha256 vector")
assert(not pcall(crypto.hmac, "__NOPE__", "k", "d", "hex"), "unknown digest should error")

assert(#crypto.randomBytes(16) == 16, "randomBytes(16)")
assert(#crypto.randomBytes(32) == 32, "randomBytes(32)")

print("crypto ok")
