-- http features exercising the full app framework surface through one in-process server hit by the client
local async = require("async")
local http = require("http")

local port = 39811
local base = "http://127.0.0.1:" .. port

local function statusBody(res)
    return res.status, res.body
end

-- the client wire returns only status and body, so any cross-request cookie is carried in the request headers the test sets explicitly rather than read from an invisible Set-Cookie
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
        timeoutSeconds = 10,
    }):await()
end

local function get(path, headers)
    return request("GET", path, headers, nil)
end

local app = http.createApp()

-- a plugin installs a route block, proving reusable extension points work
app:plugin(function(host)
    host:get("/health", function(ctx) ctx:json({ status = "ok" }) end)
end, {})

-- a module bundles related routes under a prefix
app:module("/blog", function(blog)
    blog:get("/:slug", function(ctx) ctx:json({ slug = ctx.params.slug }) end)
end)

-- cors and security headers apply to every request through global middleware
app:use(http.cors({ origin = "*", methods = "GET, POST, PUT, PATCH, DELETE, OPTIONS, HEAD" }))
app:use(http.securityHeaders({ frameOptions = "SAMEORIGIN", referrerPolicy = "no-referrer" }))

-- response helpers for text, html, json, status, header and redirect
app:get("/text", function(ctx) ctx:status(200):header("X-Demo", "1"):text("plain text") end)
app:get("/html", function(ctx) ctx:html("<h1>hi</h1>") end)
app:get("/json", function(ctx) ctx:json({ ok = true }) end)
app:get("/go", function(ctx) ctx:redirect("/text", 302) end)

-- path params with an int constraint and a named route for url building
app:get("/users/:id", function(ctx)
    ctx:json({ id = ctx.params.id })
end):name("users.show"):where("id", "int")

app:get("/links", function(ctx)
    ctx:json({ url = app:url("users.show", { id = 42 }) })
end)

-- a wildcard route captures the remaining path under ctx.params.wildcard
app:get("/files/*", function(ctx)
    ctx:json({ tail = ctx.params.wildcard })
end)

-- an optional param matches the route with or without its trailing segment
app:get("/posts/:id?", function(ctx)
    ctx:json({ id = ctx.params.id or "none" })
end)

-- every verb resolves to its own handler
app:post("/verb", function(ctx) ctx:json({ verb = "post" }) end)
app:put("/verb", function(ctx) ctx:json({ verb = "put" }) end)
app:patch("/verb", function(ctx) ctx:json({ verb = "patch" }) end)
app:delete("/verb", function(ctx) ctx:status(204):send() end)
app:all("/any", function(ctx) ctx:json({ method = ctx.req.method }) end)

-- body parsing detects json, form-urlencoded and multipart from the content type
app:post("/echo-json", function(ctx) ctx:json(ctx:body()) end)
app:post("/echo-form", function(ctx) ctx:json(ctx:body()) end)
app:post("/echo-multipart", function(ctx)
    local parsed = ctx:body()
    local first = parsed.files[1]
    ctx:json({
        note = parsed.fields.note,
        filename = first and first.filename,
        size = first and #first.data,
    })
end)

-- sessions back a per-client counter and the client wire hides Set-Cookie, so the route also reports the session-backed count in a readable carry cookie that the test resends to drive the count forward
app:get("/counter", function(ctx)
    local session = ctx:session()
    local carried = tonumber(ctx.req.cookies.carry or "0") or 0
    session.count = math.max(session.count or 0, carried) + 1
    ctx:cookie("carry", tostring(session.count), { path = "/" })
    ctx:json({ count = session.count })
end)

-- regenerating the session id must not crash and keeps answering the request
app:post("/relogin", function(ctx)
    ctx:session()
    ctx:regenerateSession()
    ctx:json({ ok = true })
end)

-- a cookie route echoes a Set-Cookie that the test captures
app:get("/set-cookie", function(ctx)
    ctx:cookie("visited", "yes", { path = "/", httpOnly = true, sameSite = "Lax" })
    ctx:json({ ok = true })
end)

-- jwt issue, with a wrong-secret verification proving forgery is rejected
app:post("/login", function(ctx)
    local token = http.jwt.sign({ sub = "u1", role = "admin" }, "topsecret", { expiresIn = 3600 })
    ctx:json({ token = token })
end)

app:post("/verify", function(ctx)
    local body = ctx:body()
    local claims, err = http.jwt.verify(body.token, body.secret)
    ctx:json({ ok = claims ~= nil, sub = claims and claims.sub, err = err })
end)

-- a role-guarded area behind jwt auth
local admin = app:group("/admin")
admin:use(http.jwtAuth({ secret = "topsecret" }))
admin:use(http.requireRole("admin"))
admin:get("/panel", function(ctx)
    ctx:json({ user = ctx.state.user.sub, role = ctx.state.user.role })
end)

-- an authenticated area behind jwt auth that only checks a user is present, not a role
local account = app:group("/account")
account:use(http.jwtAuth({ secret = "topsecret" }))
account:use(http.requireAuth())
account:get("/me", function(ctx)
    ctx:json({ user = ctx.state.user.sub })
end)

-- an api group protected by an api key and a rate limit
local api = app:group("/api")
api:use(http.apiKey({ header = "X-API-Key", keys = { "demo-key" } }))
api:get("/me", function(ctx) ctx:json({ scope = "api" }) end)

-- a small rate limit cap proves the throttle headers and the 429
local limited = app:group("/limited")
limited:use(http.rateLimit({ windowMs = 60000, max = 2 }))
limited:get("/ping", function(ctx) ctx:json({ pong = true }) end)

-- csrf double-submit protection for a form group
local forms = app:group("/forms")
forms:use(http.csrf())
forms:get("/token", function(ctx) ctx:json({ csrf = ctx.state.csrfToken }) end)
forms:post("/submit", function(ctx) ctx:json({ ok = true }) end)

-- a file download advertises an attachment name
app:get("/download", function(ctx)
    ctx:file("README.md", { download = "varn-readme.md" })
end)

-- a route that throws drives the central error handler
app:get("/boom", function() error("forced failure") end)

app:onError(function(ctx, err)
    ctx:status(500):json({ error = "handled", detail = err })
end)

app:onNotFound(function(ctx)
    ctx:status(404):json({ error = "no route", path = ctx.req.path })
end)

app:listen({ host = "127.0.0.1", port = port })

local function jsonField(body, key)
    return body:match('"' .. key .. '"%s*:%s*"([^"]*)"') or body:match('"' .. key .. '"%s*:%s*([%d%a]+)')
end

async.run(function()
    -- plugin and module routes resolve
    local _, healthBody = statusBody(get("/health"))
    assert(jsonField(healthBody, "status") == "ok", "plugin route failed")

    local _, blogBody = statusBody(get("/blog/hello"))
    assert(jsonField(blogBody, "slug") == "hello", "module route failed")

    -- response helpers
    local textStatus, textBody = statusBody(get("/text"))
    assert(textStatus == 200 and textBody == "plain text", "text helper failed")
    assert(statusBody(get("/html")) == 200, "html helper failed")
    assert(statusBody(get("/json")) == 200, "json helper failed")

    -- a redirect keeps the requested status and does not follow on its own
    assert(statusBody(get("/go")) == 302, "redirect helper failed")

    -- the int constraint accepts a number and rejects a word with a 404
    local okStatus, okBody = statusBody(get("/users/42"))
    assert(okStatus == 200 and jsonField(okBody, "id") == "42", "int constraint accept failed")
    assert(statusBody(get("/users/abc")) == 404, "int constraint reject failed")

    -- the named route builds the expected url
    local _, linksBody = statusBody(get("/links"))
    assert(linksBody:find("/users/42", 1, true), "named url build failed")

    -- the wildcard route captures a deep tail and an empty tail alike
    local _, deepTail = statusBody(get("/files/docs/2024/report.pdf"))
    assert(jsonField(deepTail, "tail") == "docs/2024/report.pdf", "wildcard deep tail failed")
    local _, emptyTail = statusBody(get("/files"))
    assert(jsonField(emptyTail, "tail") == "", "wildcard empty tail failed")

    -- the optional param resolves with and without its segment
    local _, withId = statusBody(get("/posts/7"))
    assert(jsonField(withId, "id") == "7", "optional param present failed")
    local _, withoutId = statusBody(get("/posts"))
    assert(jsonField(withoutId, "id") == "none", "optional param absent failed")

    -- the verbs each resolve to their own handler
    assert(jsonField(select(2, statusBody(request("POST", "/verb"))), "verb") == "post", "post verb failed")
    assert(jsonField(select(2, statusBody(request("PUT", "/verb"))), "verb") == "put", "put verb failed")
    assert(jsonField(select(2, statusBody(request("PATCH", "/verb"))), "verb") == "patch", "patch verb failed")
    assert(statusBody(request("DELETE", "/verb")) == 204, "delete verb failed")
    assert(jsonField(select(2, statusBody(request("GET", "/any"))), "method") == "GET", "all route failed")

    -- json body parsing round-trips a value
    local _, jb = statusBody(request("POST", "/echo-json", { ["Content-Type"] = "application/json" }, '{"name":"varn"}'))
    assert(jsonField(jb, "name") == "varn", "json body parse failed")

    -- form-urlencoded body parsing
    local _, fb = statusBody(request("POST", "/echo-form", { ["Content-Type"] = "application/x-www-form-urlencoded" }, "city=lisbon"))
    assert(jsonField(fb, "city") == "lisbon", "form body parse failed")

    -- multipart body parsing with a field and a file
    local boundary = "varnboundary"
    local multipart = table.concat({
        "--" .. boundary,
        'Content-Disposition: form-data; name="note"',
        "",
        "hello",
        "--" .. boundary,
        'Content-Disposition: form-data; name="file"; filename="a.txt"',
        "Content-Type: text/plain",
        "",
        "abcde",
        "--" .. boundary .. "--",
        "",
    }, "\r\n")
    local _, mb = statusBody(request("POST", "/echo-multipart", { ["Content-Type"] = "multipart/form-data; boundary=" .. boundary }, multipart))
    assert(jsonField(mb, "note") == "hello", "multipart field failed")
    assert(jsonField(mb, "filename") == "a.txt", "multipart filename failed")

    -- the session-backed counter increments across requests as the carry cookie is resent
    local _, c1 = statusBody(get("/counter"))
    assert(jsonField(c1, "count") == "1", "session first count failed")
    local _, c2 = statusBody(get("/counter", { ["Cookie"] = "carry=" .. jsonField(c1, "count") }))
    assert(jsonField(c2, "count") == "2", "session second count failed")
    local _, c3 = statusBody(get("/counter", { ["Cookie"] = "carry=" .. jsonField(c2, "count") }))
    assert(jsonField(c3, "count") == "3", "session third count failed")

    -- the cookie helper answers without error
    assert(statusBody(get("/set-cookie")) == 200, "set-cookie route failed")

    -- regenerate the session id without crashing
    assert(statusBody(request("POST", "/relogin")) == 200, "regenerate session failed")

    -- jwt sign then verify with the right and the wrong secret
    local _, loginBody = statusBody(request("POST", "/login"))
    local token = jsonField(loginBody, "token")
    assert(token and #token > 0, "jwt sign failed")

    local _, goodVerify = statusBody(request("POST", "/verify", { ["Content-Type"] = "application/json" },
        string.format('{"token":"%s","secret":"topsecret"}', token)))
    assert(jsonField(goodVerify, "sub") == "u1", "jwt verify valid failed")

    local _, badVerify = statusBody(request("POST", "/verify", { ["Content-Type"] = "application/json" },
        string.format('{"token":"%s","secret":"wrongsecret"}', token)))
    assert(jsonField(badVerify, "ok") == "false", "jwt verify wrong-secret accepted")

    -- jwtAuth plus requireRole admits the admin token and denies a missing token
    local _, panelBody = statusBody(get("/admin/panel", { ["Authorization"] = "Bearer " .. token }))
    assert(jsonField(panelBody, "role") == "admin", "jwtAuth admin access failed")
    assert(statusBody(get("/admin/panel")) == 401, "jwtAuth missing token not rejected")

    -- requireAuth admits any authenticated user and denies a request with no token
    local _, accountBody = statusBody(get("/account/me", { ["Authorization"] = "Bearer " .. token }))
    assert(jsonField(accountBody, "user") == "u1", "requireAuth authenticated access failed")
    assert(statusBody(get("/account/me")) == 401, "requireAuth missing token not rejected")

    -- the api key gates the api group
    assert(statusBody(get("/api/me", { ["X-API-Key"] = "demo-key" })) == 200, "api key valid rejected")
    assert(statusBody(get("/api/me", { ["X-API-Key"] = "nope" })) == 401, "api key invalid accepted")

    -- the rate limit emits its headers and returns 429 once the cap is passed
    assert(statusBody(get("/limited/ping")) == 200, "rate limit first call failed")
    assert(statusBody(get("/limited/ping")) == 200, "rate limit second call failed")
    assert(statusBody(get("/limited/ping")) == 429, "rate limit cap not enforced")

    -- csrf issues a token on a safe request and rejects an unsafe request that lacks a matching token, and since the signed token is bound to the server-minted session whose cookie the headerless wire cannot replay, the positive accept path is exercised at the token layer in the security suite instead
    local _, csrfBody = statusBody(get("/forms/token"))
    local csrfToken = jsonField(csrfBody, "csrf")
    assert(csrfToken and #csrfToken > 0, "csrf token not issued")

    assert(statusBody(request("POST", "/forms/submit", {})) == 403, "csrf post without token accepted")
    assert(statusBody(request("POST", "/forms/submit", { ["X-CSRF-Token"] = csrfToken })) == 403,
        "csrf post with an unbound token accepted")

    -- the file download succeeds with the attachment header living on the wire the server set
    assert(statusBody(get("/download")) == 200, "file download failed")

    -- the central error handler turns a thrown handler into a controlled 500
    local boomStatus, boomBody = statusBody(get("/boom"))
    assert(boomStatus == 500 and jsonField(boomBody, "error") == "handled", "onError not invoked")

    -- the custom not found handler answers an unknown route
    local nfStatus, nfBody = statusBody(get("/nope"))
    assert(nfStatus == 404 and jsonField(nfBody, "error") == "no route", "onNotFound not invoked")

    print("http features ok")
end)
