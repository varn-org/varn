-- JWT verification passes a valid token and refuses every bad one, a wrong secret, a forged `alg=none` token, a tampered payload, an expired token and a not-yet-valid one among them, with the sentence that names the reason.
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

-- Calls `verify` and asserts that it refuses the token with the sentence that names the reason.
local function refuses(token, wanted, opts)
    local refused, err = http.jwt.verify(token, secret, opts)
    assert(refused == nil, "The token should be refused with: " .. wanted)
    assert(err == wanted, "Expected the refusal " .. wanted .. " but got " .. tostring(err))
end

-- The wrong secret is rejected.
local wrong, wrongErr = http.jwt.verify(good, "wrongsecret")
assert(wrong == nil and wrongErr == "The signature of the token does not match the secret.", "A wrong secret should be rejected, got " .. tostring(wrongErr))

-- A forged `alg=none` token with no signature must be rejected, never trusted.
local forged = b64url({ alg = "none", typ = "JWT" }) .. "." .. b64url({ sub = "admin" }) .. "."
refuses(forged, 'The token is not signed with the algorithm "HS256".')

-- A tampered payload no longer matches the signature and must be rejected.
local h, p, s = good:match("^([^.]+)%.([^.]+)%.([^.]+)$")
assert(h and p and s, "A signed token should have three segments")
local payload = json.decode(crypto.base64UrlDecode(p))
payload.role = "superadmin"
local tampered = h .. "." .. crypto.base64UrlEncode(json.encode(payload)) .. "." .. s
refuses(tampered, "The signature of the token does not match the secret.")

-- An expired token is rejected.
refuses(http.jwt.sign({ sub = "u1", exp = os.time() - 100 }, secret), "The token expired.")

-- A token that is not valid yet is rejected.
refuses(http.jwt.sign({ sub = "u1", nbf = os.time() + 100000 }, secret), "The token is not valid yet.")

-- A token without its parts, a header that is not an object and a header with critical extensions are refused by name.
refuses("only-one-part", "The token is not a header, a payload and a signature separated by dots.")
refuses(crypto.base64UrlEncode('"HS256"') .. "." .. b64url({ sub = "u1" }) .. ".x", "The header of the token is not a JSON object.")
refuses(b64url({ alg = "HS256", crit = { "exp" } }) .. "." .. b64url({ sub = "u1" }) .. ".x", "The token names critical extensions in its header, which this verifier does not process.")

-- A claim of the wrong type, an audience and an issuer that do not match are refused by name.
refuses(http.jwt.sign({ sub = "u1", exp = "soon" }, secret), 'The claim "exp" of the token is not a number.')
refuses(good, 'The claim "aud" of the token does not hold the expected audience.', { audience = "api" })
refuses(good, 'The claim "iss" of the token does not name the expected issuer.', { issuer = "varn" })
assert(http.jwt.verify(http.jwt.sign({ sub = "u1", aud = { "web", "api" }, iss = "varn" }, secret), secret, { audience = "api", issuer = "varn" }), "A matching audience and issuer should verify")

-- An option of the wrong type is refused by name instead of skipping the check it asks for.
local okAudience, audienceErr = pcall(http.jwt.verify, good, secret, { audience = 5 })
assert(not okAudience and tostring(audienceErr):find('The option "audience" must be a string.', 1, true), "A mistyped audience should be refused, got " .. tostring(audienceErr))
local okLeeway, leewayErr = pcall(http.jwt.verify, good, secret, { leeway = -1 })
assert(not okLeeway and tostring(leewayErr):find('The option "leeway" must be a whole number of seconds of at least 0.', 1, true), "A negative leeway should be refused, got " .. tostring(leewayErr))
local okAuth, authErr = pcall(http.jwtAuth, { secret = secret, issuer = {} })
assert(not okAuth and tostring(authErr):find('The option "issuer" must be a string.', 1, true), "A mistyped option of the middleware should be refused, got " .. tostring(authErr))

-- A token without a secret is refused before anything is decoded.
local unsecured, unsecuredErr = http.jwt.verify(good, "")
assert(unsecured == nil and unsecuredErr == "The token cannot be verified without a secret.", "An empty secret should be refused, got " .. tostring(unsecuredErr))

print("The \"http\" JWT security tests passed.")
