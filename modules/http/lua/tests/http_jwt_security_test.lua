-- JWT verification passes a valid token while a wrong secret, a forged `alg=none` token, a tampered payload, an expired token and a not-yet-valid token are all rejected.
local http = require("http")
local crypto = require("crypto")
local json = require("json")

local secret = "topsecret"

local function b64url(value)
    return crypto.base64UrlEncode(json.encode(value))
end

-- A valid token verifies and returns its claims.
local good = http.jwt.sign({ sub = "u1", role = "admin" }, secret, { expiresIn = 3600 })
local claims = http.jwt.verify(good, secret)
assert(claims and claims.sub == "u1" and claims.role == "admin", "A valid token should verify")

-- The wrong secret is rejected.
assert(not http.jwt.verify(good, "wrongsecret"), "A wrong secret should be rejected")

-- A forged `alg=none` token with no signature must be rejected, never trusted.
local forged = b64url({ alg = "none", typ = "JWT" }) .. "." .. b64url({ sub = "admin" }) .. "."
assert(not http.jwt.verify(forged, secret), 'An "alg=none" token should be rejected')

-- A tampered payload no longer matches the signature and must be rejected.
local h, p, s = good:match("^([^.]+)%.([^.]+)%.([^.]+)$")
assert(h and p and s, "A signed token should have three segments")
local payload = json.decode(crypto.base64UrlDecode(p))
payload.role = "superadmin"
local tampered = h .. "." .. crypto.base64UrlEncode(json.encode(payload)) .. "." .. s
assert(not http.jwt.verify(tampered, secret), "A tampered payload should be rejected")

-- An expired token is rejected.
local expired = http.jwt.sign({ sub = "u1", exp = os.time() - 100 }, secret)
assert(not http.jwt.verify(expired, secret), "An expired token should be rejected")

-- A token that is not valid yet is rejected.
local future = http.jwt.sign({ sub = "u1", nbf = os.time() + 100000 }, secret)
assert(not http.jwt.verify(future, secret), "A not-yet-valid token should be rejected")

print("The \"http\" JWT security tests passed.")
