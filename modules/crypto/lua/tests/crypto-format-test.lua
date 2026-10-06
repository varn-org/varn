-- The output format of a digest is "hex" or "raw", and any other value is refused rather than read as hex.
local crypto = require("crypto")

local hex = crypto.digest("SHA256", "abc")
assert(crypto.digest("SHA256", "abc", "hex") == hex, 'The format "hex" is the default')
assert(crypto.digest("SHA256", "abc", nil) == hex, "A nil format is the default")
assert(#crypto.digest("SHA256", "abc", "raw") == 32, 'The format "raw" answers the bytes')

for _, format in ipairs({ true, 1, {}, "HEX", "base64" }) do
    local ok, err = pcall(crypto.digest, "SHA256", "abc", format)
    assert(not ok and tostring(err):find('Output format must be "hex" or "raw".', 1, true), "The format " .. tostring(format) .. " should be refused, got " .. tostring(err))

    local okMac, errMac = pcall(crypto.hmac, "SHA256", "key", "abc", format)
    assert(not okMac and tostring(errMac):find('Output format must be "hex" or "raw".', 1, true), "The keyed hash format " .. tostring(format) .. " should be refused")
end

-- Both base64 decoders read either alphabet.
assert(crypto.base64UrlDecode(crypto.base64Encode("\251\255")) == "\251\255", "The URL decoder reads the standard alphabet")
assert(crypto.base64Decode(crypto.base64UrlEncode("\251\255")) == "\251\255", "The standard decoder reads the URL alphabet")

-- A failure of a key derivation reads as a sentence with every word apart.
local ok, err = pcall(crypto.hkdf, "ikm", "salt", "info", 1, "NO-SUCH-DIGEST")
assert(not ok and tostring(err):find("not known", 1, true), "An unknown digest should be refused, got " .. tostring(err))

print("The \"crypto\" format tests passed.")
