-- jwt verification passes a valid token while a wrong secret, a forged alg=none token, a tampered payload, an expired token and a not-yet-valid token are all rejected
local http = require("http")
local crypto = require("crypto")
local json = require("json")

local secret = "topsecret"

local function b64url(value)
    return crypto.base64UrlEncode(json.encode(value))
end

-- a valid token verifies and returns its claims
local good = http.jwt.sign({ sub = "u1", role = "admin" }, secret, { expiresIn = 3600 })
local claims = http.jwt.verify(good, secret)
assert(claims and claims.sub == "u1" and claims.role == "admin", "a valid token should verify")

-- the wrong secret is rejected
assert(not http.jwt.verify(good, "wrongsecret"), "a wrong secret should be rejected")

-- a forged alg=none token with no signature must be rejected, never trusted
local forged = b64url({ alg = "none", typ = "JWT" }) .. "." .. b64url({ sub = "admin" }) .. "."
assert(not http.jwt.verify(forged, secret), "an alg=none token should be rejected")

-- a tampered payload no longer matches the signature and must be rejected
local h, p, s = good:match("^([^.]+)%.([^.]+)%.([^.]+)$")
assert(h and p and s, "a signed token should have three segments")
local payload = json.decode(crypto.base64UrlDecode(p))
payload.role = "superadmin"
local tampered = h .. "." .. crypto.base64UrlEncode(json.encode(payload)) .. "." .. s
assert(not http.jwt.verify(tampered, secret), "a tampered payload should be rejected")

-- an expired token is rejected
local expired = http.jwt.sign({ sub = "u1", exp = os.time() - 100 }, secret)
assert(not http.jwt.verify(expired, secret), "an expired token should be rejected")

-- a token that is not valid yet is rejected
local future = http.jwt.sign({ sub = "u1", nbf = os.time() + 100000 }, secret)
assert(not http.jwt.verify(future, secret), "a not-yet-valid token should be rejected")

print("http jwt security ok")
