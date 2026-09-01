-- full tour of the http app framework covering routing, groups, middleware, params, constraints, named routes, cookies, sessions, body parsing, uploads, downloads and static files
local http = require("http")

local app = http.createApp()

-- config is a simple key/value store readable anywhere the app is in scope
app:config({ appName = "Varn app", version = "1.0.0" })
app:config("env", os.getenv("VARN_ENV") or "dev")

-- onStart runs setup when the server starts
app:onStart(function()
    print("app started: " .. app:config("appName") .. " " .. app:config("version"))
end)

-- request/response hooks observe every request (hooks observe, middleware controls flow)
app:onRequest(function(ctx)
    ctx.state.startedAt = os.clock()
end)
app:onResponse(function(ctx)
    print(string.format("done %s %s", ctx.req.method, ctx.req.path))
end)

-- the event bus decouples side effects from handlers
app:on("user.created", function(name)
    print("event user.created:", name)
end)

-- a plugin is a reusable block that installs routes, middleware and handlers into the app
local function healthPlugin(host, opts)
    host:get(opts.path or "/health", function(ctx)
        ctx:json({ status = "ok" })
    end)
end
app:plugin(healthPlugin, { path = "/health" })

-- a module bundles a group of related routes under a prefix
app:module("/blog", function(blog)
    blog:get("/", function(ctx) ctx:json({ posts = {} }) end)
    blog:get("/:slug", function(ctx) ctx:json({ slug = ctx.params.slug }) end)
end)

-- global middleware runs on every request and can act before and after the handler
app:use(function(ctx, next)
    ctx.state.requestId = tostring(math.random(100000, 999999))
    next()
    print(string.format("[%s] %s %s", ctx.state.requestId, ctx.req.method, ctx.req.path))
end)

-- built-in security middlewares cors and security headers apply to every request, where origin may be "*" or an allowlist table that echoes only matching request origins and credentials combined with origin "*" is rejected at setup, never silently
app:use(http.cors({ origin = "*", methods = "GET, POST, PUT, PATCH, DELETE, OPTIONS, HEAD" }))
app:use(http.securityHeaders({ frameOptions = "SAMEORIGIN", referrerPolicy = "no-referrer", hsts = 31536000 }))

-- a per-route middleware only runs for the routes it is attached to
local function requireToken(ctx, next)
    if ctx.req.headers["X-Token"] ~= "secret" then
        ctx:status(401):json({ error = "missing token" })
        return
    end
    next()
end

-- response helpers for json, text, html, status, header and redirect
app:get("/", function(ctx)
    ctx:html("<h1>Varn app</h1>")
end)

app:get("/text", function(ctx)
    ctx:status(200):header("X-Demo", "1"):text("plain text")
end)

app:get("/go", function(ctx)
    ctx:redirect("/", 302)
end)

-- path params plus a constraint and a named route for url building
app:get("/users/:id", function(ctx)
    ctx:json({ id = ctx.params.id, query = ctx.query })
end):name("users.show"):where("id", "int")

app:get("/links", function(ctx)
    ctx:json({ user = app:url("users.show", { id = 42 }) })
end)

-- every verb is available, plus all() and route()
app:put("/items/:id", function(ctx) ctx:json({ updated = ctx.params.id }) end)
app:delete("/items/:id", function(ctx) ctx:status(204):send() end)
app:all("/any", function(ctx) ctx:text("method was " .. ctx.req.method) end)

-- route groups share a prefix and their own middleware (also used for versioning), and this group enforces an api key and a rate limit on top of the custom token check
local api = app:group("/api/v1")
api:use(http.rateLimit({ windowMs = 60000, max = 100 }))
api:use(http.apiKey({ header = "X-API-Key", keys = { "demo-key" } }))
api:use(requireToken)
api:get("/me", function(ctx)
    ctx:json({ id = ctx.state.requestId, scope = "api" })
end)
api:post("/items", function(ctx)
    local body = ctx:body()
    ctx:status(201):json({ created = body })
end)

-- body parsing detects json, form-urlencoded and multipart from the content type
app:post("/form", function(ctx)
    ctx:json(ctx:body())
end)

app:post("/upload", function(ctx)
    local parsed = ctx:body()
    local first = parsed.files[1]
    ctx:json({
        note = parsed.fields.note,
        filename = first and first.filename,
        contentType = first and first.contentType,
        size = first and #first.data,
    })
end)

-- cookies are read from ctx.req.cookies and written with ctx:cookie
app:get("/cookie", function(ctx)
    ctx:cookie("visited", "yes", { path = "/", httpOnly = true, maxAge = 3600, sameSite = "Lax" })
    ctx:json({ previous = ctx.req.cookies.visited })
end)

-- sessions persist per client across requests using an in-memory store
app:get("/counter", function(ctx)
    local session = ctx:session()
    session.count = (session.count or 0) + 1
    ctx:json({ count = session.count })
end)

-- regenerate the session id on a privilege change to defeat session fixation
app:post("/session-login", function(ctx)
    local session = ctx:session()
    session.user = "u1"
    ctx:regenerateSession()
    ctx:json({ ok = true })
end)

-- file download with an attachment name
app:get("/download", function(ctx)
    ctx:file("README.md", { download = "varn-readme.md" })
end)

-- jwt issue + verify, with a role guarded area
app:post("/login", function(ctx)
    local token = http.jwt.sign({ sub = "u1", role = "admin" }, "topsecret", { expiresIn = 3600 })
    ctx:json({ token = token })
end)

local admin = app:group("/admin")
admin:use(http.jwtAuth({ secret = "topsecret" }))
admin:use(http.requireRole("admin"))
admin:get("/panel", function(ctx)
    ctx:json({ user = ctx.state.user.sub, role = ctx.state.user.role })
end)

-- csrf double-submit protection for browser forms
local forms = app:group("/forms")
forms:use(http.csrf())
forms:get("/token", function(ctx) ctx:json({ csrf = ctx.state.csrfToken }) end)
forms:post("/submit", function(ctx) ctx:json({ ok = true }) end)

-- streaming sends chunks progressively with chunked transfer encoding
local async = require("async")
app:get("/stream", function(ctx)
    ctx:type("text/plain")
    for i = 1, 5 do
        ctx:write("chunk " .. i .. "\n")
        async.sleep(100):await()
    end
    ctx:send()
end)

-- websocket endpoint with open/message/close callbacks (server owns the socket, lua stays on its thread)
app:ws("/ws", {
    open = function(conn) conn:send("welcome") end,
    message = function(conn, data)
        if data == "bye" then
            conn:send("closing")
            conn:close()
        else
            conn:send("echo:" .. data)
        end
    end,
    close = function() print("websocket closed") end,
})

-- the same endpoint with an origin allowlist that blocks cross-site upgrades
app:ws("/ws-secure", {
    origins = { "http://localhost:3000", "https://localhost:3000" },
    open = function(conn) conn:send("welcome") end,
    message = function(conn, data) conn:send("echo:" .. data) end,
    close = function() print("secure websocket closed") end,
})

-- centralized error handling and a custom not found page
app:get("/boom", function()
    error("something failed")
end)

app:onError(function(ctx, err)
    ctx:status(500):json({ error = err, requestId = ctx.state.requestId })
end)

app:onNotFound(function(ctx)
    ctx:status(404):json({ error = "no route", path = ctx.req.path })
end)

-- static files are served from publicDir with cache, range and directory listing support, and requestTimeoutMs bounds how long a handler may run before the server answers 504
app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    servePublic = true,
    directoryListing = true,
    requestTimeoutMs = 30000,
})
