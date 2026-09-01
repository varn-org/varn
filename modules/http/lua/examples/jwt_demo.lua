-- demonstrates issuing and verifying json web tokens without a server, then exits cleanly
local http = require("http")

local secret = "topsecret"

-- sign a short-lived token carrying a subject and a role claim
local token = http.jwt.sign({ sub = "u1", role = "admin" }, secret, { expiresIn = 3600 })
print("token issued, length " .. #token)

-- verifying with the right secret returns the decoded claims
local claims, err = http.jwt.verify(token, secret)
assert(claims, err)
print("verified sub=" .. claims.sub .. " role=" .. claims.role)

-- verifying with the wrong secret returns nil and an error, never the claims
local forged, forgedErr = http.jwt.verify(token, "not-the-secret")
print("wrong secret rejected: " .. tostring(forged == nil) .. " (" .. tostring(forgedErr) .. ")")

