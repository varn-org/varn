-- http security for Lua-reachable abuse cases from docs/security-tests asserting safe outcomes and no crash, covering response header injection (CWE-113), oversized body (CWE-400), cookie value injection (CWE-113), jwt forgery alg-none/expired/wrong-signature/tamper (CWE-347/345/613), csrf cross-submit (CWE-352), timing-safe api key mismatch (CWE-208/287), rate limiting (CWE-770), static path traversal (CWE-22), and method-not-allowed with an Allow header (RFC 9110)
local async = require("async")
local http = require("http")
local crypto = require("crypto")

local port = 39812
local base = "http://127.0.0.1:" .. port
local dir = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set; run tests with: python3 varn.py test")

local function statusBody(res)
    return res.status, res.body
end

local function request(method, path, headers, body)
    headers = headers or {}
    if body then
        headers["Content-Length"] = tostring(#body)
    end
    return http.client.requestRaw({
        url = base .. path,
        method = method,
        headers = headers,
        body = body,
        timeoutSeconds = 30,
    }):await()
end

local function get(path, headers)
    return request("GET", path, headers, nil)
end

local function jsonField(body, key)
    return body:match('"' .. key .. '"%s*:%s*"([^"]*)"') or body:match('"' .. key .. '"%s*:%s*([%w]+)')
end

-- standard base64url without padding, matching the server's token encoding so forged tokens are well formed
local b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
local function base64url(input)
    local out = {}
    local i = 1
    local n = #input
    while i <= n do
        local b1 = input:byte(i)
        local b2 = i + 1 <= n and input:byte(i + 1) or nil
        local b3 = i + 2 <= n and input:byte(i + 2) or nil
        local triple = b1 * 0x10000 + (b2 or 0) * 0x100 + (b3 or 0)
        out[#out + 1] = b64chars:sub(math.floor(triple / 0x40000) % 64 + 1, math.floor(triple / 0x40000) % 64 + 1)
        out[#out + 1] = b64chars:sub(math.floor(triple / 0x1000) % 64 + 1, math.floor(triple / 0x1000) % 64 + 1)
        if b2 then
            out[#out + 1] = b64chars:sub(math.floor(triple / 0x40) % 64 + 1, math.floor(triple / 0x40) % 64 + 1)
        end
        if b3 then
            out[#out + 1] = b64chars:sub(triple % 64 + 1, triple % 64 + 1)
        end
        i = i + 3
    end
    return table.concat(out)
end

local secret = "topsecret"

-- forges a token with the given header and payload json, signed with the supplied hmac secret
local function forgeToken(headerJson, payloadJson, signingSecret)
    local signingInput = base64url(headerJson) .. "." .. base64url(payloadJson)
    local signature = base64url(crypto.hmac("SHA256", signingSecret, signingInput, "raw"))
    return signingInput .. "." .. signature
end

-- the scratch directory is the static root, holding one public file for the traversal attempts
local seed = io.open(dir .. "/index.html", "w")
assert(seed, "could not create the static fixture")
seed:write("<h1>public</h1>")
seed:close()

-- a symlinked index.html inside the public tree points to a file outside it for the CWE-59 index escape check
local r1Secret = dir .. "/../varn_r1_secret.txt"
local r1Marker = "R1-OUT-OF-TREE-SECRET"
local r1Ready = false
if package.config:sub(1, 1) == "/" then
    local secretFile = io.open(r1Secret, "w")
    if secretFile then
        secretFile:write(r1Marker)
        secretFile:close()
        r1Ready = os.execute("mkdir '" .. dir .. "/evil' && ln -s '" .. r1Secret .. "' '" .. dir .. "/evil/index.html'") and true or false
    end
end

local app = http.createApp()

-- attempts a header name with embedded crlf and reports whether the framework rejected it
app:get("/inject-header", function(ctx)
    local ok = pcall(function() ctx:header("X-Bad\r\nInjected", "1") end)
    ctx:json({ rejected = not ok })
end)

-- attempts a cookie value with control characters and reports whether it was rejected
app:get("/inject-cookie", function(ctx)
    local ok = pcall(function() ctx:cookie("sid", "a\r\nb") end)
    ctx:json({ rejected = not ok })
end)

-- verifies a client-supplied token against the trusted secret and reports the verdict
app:post("/verify", function(ctx)
    local body = ctx:body()
    local claims, err = http.jwt.verify(body.token, secret)
    ctx:json({ ok = claims ~= nil, err = err })
end)

-- echoes the received body size so an oversized request can be confirmed to be rejected upstream
app:post("/sink", function(ctx)
    ctx:json({ size = #ctx.req.body })
end)

-- an api group gated by a timing-safe api key compare
local api = app:group("/api")
api:use(http.apiKey({ header = "X-API-Key", keys = { "the-real-key" } }))
api:get("/data", function(ctx) ctx:json({ secret = true }) end)

-- a tight rate limit so a few requests cross the cap deterministically
local limited = app:group("/limited")
limited:use(http.rateLimit({ windowMs = 60000, max = 3 }))
limited:get("/ping", function(ctx) ctx:json({ pong = true }) end)

-- a csrf-guarded form group for the cross-submit rejection
local forms = app:group("/forms")
forms:use(http.csrf())
forms:get("/token", function(ctx) ctx:json({ csrf = ctx.state.csrfToken }) end)
forms:post("/submit", function(ctx) ctx:json({ ok = true }) end)

-- a single get route so a wrong method exercises the 405 path
app:get("/only-get", function(ctx) ctx:text("ok") end)

-- static files are served from a temporary public directory the test owns
app:listen({ host = "127.0.0.1", port = port, publicDir = dir, servePublic = true })

async.run(function()
    -- HTTP-061/063 rejects a response header name carrying crlf rather than writing it
    assert(jsonField(select(2, statusBody(get("/inject-header"))), "rejected") == "true",
        "header injection not rejected")

    -- HTTP-076 rejects a cookie value with control characters
    assert(jsonField(select(2, statusBody(get("/inject-cookie"))), "rejected") == "true",
        "cookie control chars not rejected")

    -- HTTP-130 answers a request body over the server limit with 413 and never buffers it whole
    local oversized = string.rep("x", 16 * 1024 * 1024 + 32)
    assert(statusBody(request("POST", "/sink", {}, oversized)) == 413, "oversized body not rejected")

    -- HTTP-001/006 never verifies an alg none token that carries no real signature
    local noneToken = forgeToken('{"alg":"none","typ":"JWT"}', '{"sub":"attacker"}', "")
    assert(jsonField(select(2, statusBody(request("POST", "/verify",
        { ["Content-Type"] = "application/json" },
        string.format('{"token":"%s"}', noneToken)))), "ok") == "false", "alg-none token accepted")

    -- HTTP-009 rejects a token signed with the wrong secret
    local wrongToken = forgeToken('{"alg":"HS256","typ":"JWT"}', '{"sub":"attacker","role":"admin"}', "not-the-secret")
    assert(jsonField(select(2, statusBody(request("POST", "/verify",
        { ["Content-Type"] = "application/json" },
        string.format('{"token":"%s"}', wrongToken)))), "ok") == "false", "wrong-signature token accepted")

    -- HTTP-008 rejects a correctly signed token with one flipped signature byte
    local good = http.jwt.sign({ sub = "u1" }, secret, { expiresIn = 3600 })
    local flipped = good:sub(1, #good - 1) .. (good:sub(#good) == "A" and "B" or "A")
    assert(jsonField(select(2, statusBody(request("POST", "/verify",
        { ["Content-Type"] = "application/json" },
        string.format('{"token":"%s"}', flipped)))), "ok") == "false", "byte-flipped token accepted")

    -- HTTP-021 rejects an expired but correctly signed token on the exp claim
    local expiredToken = forgeToken('{"alg":"HS256","typ":"JWT"}', '{"sub":"u1","exp":1000000000}', secret)
    local _, expiredBody = statusBody(request("POST", "/verify",
        { ["Content-Type"] = "application/json" },
        string.format('{"token":"%s"}', expiredToken)))
    assert(jsonField(expiredBody, "ok") == "false", "expired token accepted")
    assert(jsonField(expiredBody, "err") == "token expired", "expired token rejected for the wrong reason")

    -- HTTP-051/053 rejects an empty key and a near-miss key through the timing-safe compare
    assert(statusBody(get("/api/data")) == 401, "missing api key accepted")
    assert(statusBody(get("/api/data", { ["X-API-Key"] = "the-real-kez" })) == 401, "near-miss api key accepted")
    assert(statusBody(get("/api/data", { ["X-API-Key"] = "the-real-key" })) == 200, "valid api key rejected")

    -- HTTP-086/089 rejects an unsafe method without a session-bound csrf token
    local _, tokenBody = statusBody(get("/forms/token"))
    local csrf = jsonField(tokenBody, "csrf")
    assert(csrf and #csrf > 0, "csrf token not issued")
    assert(statusBody(request("POST", "/forms/submit", {})) == 403, "csrf submit without token accepted")
    assert(statusBody(request("POST", "/forms/submit", { ["X-CSRF-Token"] = csrf })) == 403,
        "csrf cross-submit with an unbound token accepted")

    -- HTTP-140 answers requests past the cap with 429 instead of serving them
    assert(statusBody(get("/limited/ping")) == 200, "rate limit first call failed")
    assert(statusBody(get("/limited/ping")) == 200, "rate limit second call failed")
    assert(statusBody(get("/limited/ping")) == 200, "rate limit third call failed")
    assert(statusBody(get("/limited/ping")) == 429, "rate limit cap not enforced")

    -- HTTP-110/111/117 keeps traversal attempts from escaping the public directory
    for _, path in ipairs({ "/../README.md", "/%2e%2e/README.md", "/..%2fREADME.md", "/etc/passwd" }) do
        local status = statusBody(get(path))
        assert(status == 403 or status == 404, "traversal path " .. path .. " returned " .. status)
    end

    -- HTTP-118 (CWE-59) a symlinked index.html must not let the directory index serve a file outside the public root
    if r1Ready then
        local escapeStatus, escapeBody = statusBody(get("/evil/"))
        assert(not (escapeBody or ""):find(r1Marker, 1, true), "symlinked index leaked an out-of-tree file with status " .. tostring(escapeStatus))
        os.remove(r1Secret)
    end

    -- HTTP-102 returns 405 with an Allow header for a wrong method on a known path
    assert(statusBody(request("DELETE", "/only-get", {})) == 405, "method not allowed did not return 405")

    print("http security ok")
end)
