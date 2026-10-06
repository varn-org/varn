-- Checks known digest and HMAC vectors, digest sizes, and random byte lengths.
local crypto = require("crypto")

-- The RFC 6234 test vector for the SHA-256 of "abc" matches.
assert(crypto.digest("SHA256", "abc", "hex")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 'SHA-256 of "abc"')
assert(#crypto.digest("SHA512", "varn", "hex") == 128, "SHA-512 hex length")

-- The HMAC-SHA256 RFC 4231 case 1 vector matches.
local mac = crypto.hmac("SHA256", string.rep("\x0b", 20), "Hi There", "hex")
assert(mac == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "HMAC SHA-256 vector")
assert(not pcall(crypto.hmac, "__NOPE__", "k", "d", "hex"), "Unknown digest should error")

assert(#crypto.randomBytes(16) == 16, 'The call "randomBytes(16)"')
assert(#crypto.randomBytes(32) == 32, 'The call "randomBytes(32)"')

print("The \"crypto\" tests passed.")
