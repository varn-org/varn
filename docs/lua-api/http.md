# 🌐 http

An in-process HTTP/1.1 server (with a higher-level app framework), an HTTP client, and WebSocket support. JSON and XML response helpers are available when those modules are built.

## Server

```lua
local http = require("http")

http.createServer(function(req, res)
    res:json({ hello = req.query.name or "world" })
end):listen(3000)
```

`http.createServer(handler)` returns a builder. The handler runs once per request with `(req, res)`.

`req` fields: `host`, `method`, `path`, `target`, `queryString`, `body`, `remoteAddress`, `headers`, `cookies`, and `query` (the parsed query string as a table).

`res` methods:

- `res:status(code)` — set the response status.
- `res:setHeader(name, value)` — set a header.
- `res:finish(body?)` — send an optional body and end the response.
- `res:json(table)` — send a table as JSON.
- `res:xml(table)` — send a table as XML.

If the handler returns without ending the response, the server sends `204 No Content`.

`builder:listen(port)` or `builder:listen(options)` starts the server. The same options work for both `http.createServer` and `app:listen`: `host`, `port`, `publicDir`, `servePublic`, `directoryListing`, `requestTimeoutMs` (default 30000), `keepAliveTimeoutSeconds` (default 30), `maxQueued` (the accept backlog), `compress` (gzip responses, default true), `tls`, `certFile`, `keyFile`. The environment variables `VARN_PORT`, `VARN_TLS_CERT`, and `VARN_TLS_KEY` override the matching options. When `servePublic` is on and `publicDir` is omitted, it defaults to `apps/lua/public`.

## App framework

`http.createApp()` returns an application with routing, route groups, middleware, named routes, path constraints, sessions, cookies, body parsing, file responses, WebSockets, and built-in security middleware (`http.cors`, `http.securityHeaders`, `http.apiKey`, `http.rateLimit`, `http.csrf`, `http.jwtAuth`, `http.requireAuth`, `http.requireRole`, and `http.jwt.sign` / `http.jwt.verify`). The full tour is in the `app_full` example below.

The application carries an event bus so a handler can announce something without knowing who reacts to it. `app:on(name, handler)` subscribes and `app:emit(name, ...)` publishes, passing every extra argument through to each handler in subscription order. Delivery is synchronous and in-process, the handler list is copied before any of it runs so a handler may subscribe another one, and a handler that raises is logged and skipped rather than failing the request that emitted the event. The bus is private to one worker, so a process started with several workers does not share events between them.

`http.rateLimit(opts)` takes `windowMs` (default 60000), `max` requests per window (default 100), `trustProxy` to read the client address from `X-Forwarded-For` (default false), and `maxClients`, the number of distinct client addresses tracked (default 100000). The store is bounded on purpose: one IPv6 client can source from an entire prefix, so once `maxClients` is reached the limiter drops expired entries and, if it is still full, starts a fresh window for everyone rather than growing without end. Size it to the traffic you expect to serve, not to the traffic you expect to be attacked with.

### Context response helpers

Inside a handler `ctx` carries the request as `ctx.req` (equivalently `ctx.request`) plus the shorthands `ctx.method`, `ctx.path`, `ctx.params`, `ctx.query` and `ctx.state`, and it ends the response through `json`/`xml`/`text`/`html`/`file`/`status`/`header`/`cookie`/`type`/`redirect`/`write`/`send` (`finish` is accepted as the same call as `send`, so a handler body reads the same on `createServer` and on `createApp`). `ctx:xml(table)` is the XML twin of `ctx:json(table)`, sending `application/xml; charset=utf-8`. On top of those:

- `ctx:cache(seconds)` or `ctx:cache(opts)` — set `Cache-Control`. A number is shorthand for `public, max-age=<n>`. The options table understands `maxAge`, `sMaxAge`, `private` (default is `public`), `noStore`, `noCache`, and `mustRevalidate`. Returns `ctx` for chaining.
- `ctx:etag(value)` — set the `ETag` header (a bare value is quoted, a `W/` prefix is kept weak). When the request's `If-None-Match` matches, it answers `304` and ends the response, so guard the rest of the handler with `if ctx.req.headers["If-None-Match"] then return end`.
- `ctx:file(path, opts?)` — stream a file, with the content type taken from its extension. A path that is not an existing regular file answers `404`, so a directory, a fifo or a device is never streamed. `opts.download = true` sends it as an attachment named after the file, and `opts.download = "name.ext"` names it explicitly. Either way the name is encoded per RFC 6266, so a filename cannot break out of the header. **The path is served exactly as given** — this is the explicit escape hatch, not the guarded static handler, so a handler that builds the path from request data must contain it itself. Serving a whole directory belongs to `publicDir`, which resolves and confines every path for you.
- `ctx:accepts(type1, type2, ...)` — return the best match against the request `Accept` header, or `nil` if none fit. A bare token like `"json"` or `"html"` matches the media subtype. A full type like `"application/json"` matches exactly. A missing or `*/*` Accept header returns the first type.
- `ctx:sse()` — switch the response to `text/event-stream` with `Cache-Control: no-cache` and return a Server-Sent-Events writer:
  - `stream:send(data)` — send a default-event message. The payload may hold anything: a line break of any kind (CR, LF or CRLF, all three of which end a line in the protocol) is escaped into its own `data:` field, so text can never break out of the record.
  - `stream:send(event, data)` — send a named event.
  - `stream:comment(text)` — send a comment line, the conventional heartbeat that keeps proxies open.
  - `stream:close()` — end the stream.

  An **event name** and a **comment** each occupy a whole line of the stream, so neither may contain a line break — one would let the rest of the value forge `id:`, `retry:` or an entire further event on the client. Passing one raises, the way an invalid header name does, rather than being silently stripped. Payload data has no such restriction, since it is escaped.

  The writer builds on chunked transfer encoding, so frames flush progressively rather than buffering.

### Response compression

When the request carries `Accept-Encoding: gzip` and the response body is a compressible type (`application/json`, `application/xml`, or any `text/*`) above 1 KB, the server gzips the body and sets `Content-Encoding: gzip` plus `Vary: Accept-Encoding`. Already-encoded or tiny bodies are left alone. Compression is on by default. Pass `compress = false` to `listen` to disable it.

### WebSocket broadcast and rooms

Live connections are tracked per app so a handler can reach the others:

- `app:wsBroadcast(path, message)` — send `message` to every open connection on that ws path and return how many received it.
- inside a ws handler, `conn:join(room)` / `conn:leave(room)` manage a connection's room membership.
- `app:wsBroadcastRoom(room, message)` — send `message` to every connection in that room and return the delivered count.

Connections register on open and are removed on close, so broadcasts never touch a dead socket.

### Routing patterns

A path is a list of `/`-separated segments. A segment is either a literal, a named param, or a wildcard:

- `:name` — a named param. Its value lands in `ctx.params.name`. Add a constraint with `:where("name", "int" | "alpha" | "alnum" | "slug" | "uuid" | <regex>)`.
- `:name?` — an **optional** param. The route matches whether or not that trailing segment is present. When absent, `ctx.params.name` is `nil`. A constraint still applies when the segment is present. Example: `/posts/:id?` matches both `/posts` and `/posts/42`.
- `*` — a **wildcard** (catch-all) terminal segment. It captures the remaining path, including any `/`, and exposes it as `ctx.params.wildcard`. An empty tail is allowed. Example: `/files/*` matches `/files`, `/files/a`, and `/files/a/b/c.txt` (`wildcard` is `"a/b/c.txt"`). `app:url(name, { wildcard = "a/b" })` rebuilds the url with the tail.

## Execution model

Lua runs on a single thread (its own `lua_State`). Blocking I/O is offloaded to a worker pool and results are marshalled back. Request handlers, middleware, and WebSocket callbacks therefore run **one at a time** on that thread — like Node's event loop. Use `:await()` for I/O so the loop stays free. A handler that busy-loops or makes a synchronous blocking call will stall every other connection until it returns. HTTP requests are bounded by `requestTimeoutMs` (the server answers 504 if a handler runs too long). WebSocket messages for one connection are processed in order.

## Scaling across cores

One process runs one event loop, so it uses one CPU core. Set `VARN_WORKERS=N` to run `N` worker processes: a master forks them, each binds the same port with `SO_REUSEPORT`, and the master restarts any worker that exits. This is the model Node's `cluster` and nginx use — on Linux the kernel load-balances new connections across the workers. On Windows, which has no `fork`, the master relaunches itself as the worker processes instead. `VARN_WORKERS` defaults to `1` and is capped at `1024`. tvOS, watchOS, visionOS, and the browser have no multi-process model, so the server stays single-process there.

**Workers share nothing.** Each one is a separate process with its own `lua_State`, so anything a handler keeps in memory is private to the worker that served the request. That includes the session store behind `ctx:session()`, the CSRF secret behind `http.csrf()`, and the counters behind `http.rateLimit()`. Because the kernel spreads a client's connections across workers, a session started on one worker is invisible to the next, a CSRF token issued by one worker is rejected by another with `403`, and a rate limit of `max` per window becomes `max × N` in the worst case. The server logs an error the first time `ctx:session()` runs with `VARN_WORKERS` above `1`. Run a single worker when you need those, or keep the state in a shared store — the [redis](https://github.com/varn-org/components/blob/main/docs/redis.md) component is the usual choice.

## Request hardening

The server fails closed on ambiguous or abusive requests: duplicate or conflicting `Content-Length`/`Transfer-Encoding` headers (request smuggling) are answered with `400`, a body over 16 MB gets `413`, and a malformed chunk size is rejected. A connection that stops making progress — a slow or partial request, or a client reading its response too slowly (slowloris) — is closed once it passes `requestTimeoutMs`/`keepAliveTimeoutSeconds`.

## Client

```lua
local async = require("async")
async.run(function()
    local http = require("http")
    local resp = http.client.get("https://example.com/api", { query = { page = 2 } }):await()
    if resp.ok then
        print(resp.status, resp.json().title)
    end
end)
```

`http.client.request(options)`, `http.client.get(url, options?)` and `http.client.post(url, options?)` return a promise that resolves to a response table:

- `status` — the numeric HTTP status.
- `ok` — `true` when `status < 400`.
- `headers` — a table of the response headers with lowercased keys.
- `body` — the raw response body string.
- `json()` — parses `body` as JSON on demand (requires the `json` module).

Options: `url` (required for `request`), `method` (default `"GET"`), `headers` (table), `body` (string), `timeoutSeconds` (default `60`), `verifyTls` (default `true`), `insecure` (opt-out of TLS verification for dev certs), `maxResponseBytes` (default 64 MB), plus two ergonomic shortcuts:

- `query = { k = v }` — appended to the url as a sorted, percent-encoded query string.
- `json = value` — serialized with the `json` module and sent with `Content-Type: application/json` (unless you set that header yourself).

On failure the promise rejects with a message.

For low-level access, `http.client.requestRaw(options)` resolves to a plain `{ status, headers, body }` table without the `ok` flag or the `json()` helper. The ergonomic surface above is a thin wrapper over it.

To consume a response incrementally, `http.client.stream(options, onChunk)` invokes `onChunk` with each body chunk as it arrives (with `options.onResponse` called first with the status) and resolves once the response completes, which suits server-sent events and large downloads. `http.client.streamRaw` is its lower-level form.

## URL encoding

Percent-encoding helpers, available in every build including the browser:

- `http.urlEncode(text)` → percent-encodes `text` for a URL. Every byte outside the RFC 3986 unreserved set (`A-Za-z0-9-_.~`) becomes `%XX`, so a space becomes `%20`. Use it for a query value or a path segment. Binary-safe.
- `http.urlDecode(text)` → reverses it, turning `%XX` back into bytes and a `+` into a space, so it also decodes `application/x-www-form-urlencoded` data. Binary-safe.

```lua
local http = require("http")
local q = http.urlEncode("hello world & more")  -- "hello%20world%20%26%20more"
print(http.urlDecode(q))                         -- "hello world & more"
```

## Examples

### The full app tour

```lua
-- Full tour of the http app framework covering routing, groups, middleware, params, constraints, named routes, cookies, sessions, body parsing, uploads, downloads and static files.
local http = require("http")

local app = http.createApp()

-- Config is a simple key/value store readable anywhere the app is in scope.
app:config({ appName = "Varn app", version = "1.0.0" })
app:config("env", os.getenv("VARN_ENV") or "dev")

-- The onStart hook runs setup when the server starts.
app:onStart(function()
    print("app started: " .. app:config("appName") .. " " .. app:config("version"))
end)

-- Request/response hooks observe every request (hooks observe, middleware controls flow).
app:onRequest(function(ctx)
    ctx.state.startedAt = os.clock()
end)
app:onResponse(function(ctx)
    print(string.format("done %s %s", ctx.req.method, ctx.req.path))
end)

-- The event bus decouples side effects from handlers.
app:on("user.created", function(name)
    print("event user.created:", name)
end)

-- A plugin is a reusable block that installs routes, middleware and handlers into the app.
local function healthPlugin(host, opts)
    host:get(opts.path or "/health", function(ctx)
        ctx:json({ status = "ok" })
    end)
end
app:plugin(healthPlugin, { path = "/health" })

-- A module bundles a group of related routes under a prefix.
app:module("/blog", function(blog)
    blog:get("/", function(ctx) ctx:json({ posts = {} }) end)
    blog:get("/:slug", function(ctx) ctx:json({ slug = ctx.params.slug }) end)
end)

-- Global middleware runs on every request and can act before and after the handler.
app:use(function(ctx, next)
    ctx.state.requestId = tostring(math.random(100000, 999999))
    next()
    print(string.format("[%s] %s %s", ctx.state.requestId, ctx.req.method, ctx.req.path))
end)

-- Built-in security middlewares cors and security headers apply to every request, where origin may be "*" or an allowlist table that echoes only matching request origins and credentials combined with origin "*" is rejected at setup, never silently.
app:use(http.cors({ origin = "*", methods = "GET, POST, PUT, PATCH, DELETE, OPTIONS, HEAD" }))
app:use(http.securityHeaders({ frameOptions = "SAMEORIGIN", referrerPolicy = "no-referrer", hsts = 31536000 }))

-- A per-route middleware only runs for the routes it is attached to.
local function requireToken(ctx, next)
    if ctx.req.headers["X-Token"] ~= "secret" then
        ctx:status(401):json({ error = "missing token" })
        return
    end
    next()
end

-- Response helpers for json, text, html, status, header and redirect.
app:get("/", function(ctx)
    ctx:html("<h1>Varn app</h1>")
end)

app:get("/text", function(ctx)
    ctx:status(200):header("X-Demo", "1"):text("plain text")
end)

app:get("/go", function(ctx)
    ctx:redirect("/", 302)
end)

-- Path params plus a constraint and a named route for url building.
app:get("/users/:id", function(ctx)
    ctx:json({ id = ctx.params.id, query = ctx.query })
end):name("users.show"):where("id", "int")

app:get("/links", function(ctx)
    ctx:json({ user = app:url("users.show", { id = 42 }) })
end)

-- Every verb is available, plus all() and route().
app:put("/items/:id", function(ctx) ctx:json({ updated = ctx.params.id }) end)
app:delete("/items/:id", function(ctx) ctx:status(204):send() end)
app:all("/any", function(ctx) ctx:text("method was " .. ctx.req.method) end)

-- Route groups share a prefix and their own middleware (also used for versioning), and this group enforces an api key and a rate limit on top of the custom token check.
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

-- Body parsing detects json, form-urlencoded and multipart from the content type.
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

-- Cookies are read from ctx.req.cookies and written with ctx:cookie.
app:get("/cookie", function(ctx)
    ctx:cookie("visited", "yes", { path = "/", httpOnly = true, maxAge = 3600, sameSite = "Lax" })
    ctx:json({ previous = ctx.req.cookies.visited })
end)

-- Sessions persist per client across requests using an in-memory store.
app:get("/counter", function(ctx)
    local session = ctx:session()
    session.count = (session.count or 0) + 1
    ctx:json({ count = session.count })
end)

-- Regenerate the session id on a privilege change to defeat session fixation.
app:post("/session-login", function(ctx)
    local session = ctx:session()
    session.user = "u1"
    ctx:regenerateSession()
    ctx:json({ ok = true })
end)

-- File download with an attachment name.
app:get("/download", function(ctx)
    ctx:file("README.md", { download = "varn-readme.md" })
end)

-- Jwt issue + verify, with a role guarded area.
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

-- Csrf double-submit protection for browser forms.
local forms = app:group("/forms")
forms:use(http.csrf())
forms:get("/token", function(ctx) ctx:json({ csrf = ctx.state.csrfToken }) end)
forms:post("/submit", function(ctx) ctx:json({ ok = true }) end)

-- Streaming sends chunks progressively with chunked transfer encoding.
local async = require("async")
app:get("/stream", function(ctx)
    ctx:type("text/plain")
    for i = 1, 5 do
        ctx:write("chunk " .. i .. "\n")
        async.sleep(100):await()
    end
    ctx:send()
end)

-- Websocket endpoint with open/message/close callbacks (server owns the socket, lua stays on its thread).
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

-- The same endpoint with an origin allowlist that blocks cross-site upgrades.
app:ws("/ws-secure", {
    origins = { "http://localhost:3000", "https://localhost:3000" },
    open = function(conn) conn:send("welcome") end,
    message = function(conn, data) conn:send("echo:" .. data) end,
    close = function() print("secure websocket closed") end,
})

-- Centralized error handling and a custom not found page.
app:get("/boom", function()
    error("something failed")
end)

app:onError(function(ctx, err)
    ctx:status(500):json({ error = err, requestId = ctx.state.requestId })
end)

app:onNotFound(function(ctx)
    ctx:status(404):json({ error = "no route", path = ctx.req.path })
end)

-- Static files are served from publicDir with cache, range and directory listing support, and requestTimeoutMs bounds how long a handler may run before the server answers 504.
app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    servePublic = true,
    directoryListing = true,
    requestTimeoutMs = 30000,
})
```

### App round trip

```lua
-- Drives an in-process app with the client to show routing, body parsing and a json response then exits, with the server on its own threads so the script issues its requests inside async.run and finishes cleanly.
local async = require("async")
local http = require("http")

local port = 8091
local base = "http://127.0.0.1:" .. port

local app = http.createApp()

-- A path parameter constrained to digits feeds a json response.
app:get("/users/:id", function(ctx)
    ctx:json({ id = ctx.params.id })
end):where("id", "int")

-- Json body parsing detects the content type and hands back a parsed table.
app:post("/echo", function(ctx)
    ctx:json({ received = ctx:body() })
end)

app:listen({ host = "127.0.0.1", port = port })

async.run(function()
    local user = http.client.requestRaw({
        url = base .. "/users/7",
        method = "GET",
        headers = {},
        timeoutSeconds = 10,
    }):await()
    print("GET /users/7 -> " .. user.status .. " " .. user.body)

    local payload = '{"name":"varn"}'
    local echo = http.client.requestRaw({
        url = base .. "/echo",
        method = "POST",
        headers = { ["Content-Type"] = "application/json" },
        body = payload,
        timeoutSeconds = 10,
    }):await()
    print("POST /echo -> " .. echo.status .. " " .. echo.body)
end)
```

### Cache negotiation

```lua
-- Caching and content negotiation covering cache-control, etag revalidation, and api-vs-html responses from one route.
local http = require("http")

local app = http.createApp()

-- A long-lived cached resource sets cache-control plus an etag the client can revalidate against.
app:get("/profile/:id", function(ctx)
    local id = ctx.params.id
    ctx:cache({ maxAge = 300, private = true })
    ctx:etag("profile-" .. id .. "-v3")

    -- If etag matched the request's If-None-Match, the helper already answered 304 and ended the response.
    if ctx.req.headers["If-None-Match"] then
        return
    end

    ctx:json({ id = id, name = "User " .. id })
end)

-- The same path serves html to a browser and json to an api client based on Accept.
app:get("/report", function(ctx)
    local best = ctx:accepts("html", "json")
    if best == "json" then
        ctx:cache(60):json({ title = "Quarterly report", revenue = 1000 })
    else
        ctx:cache(60):html("<h1>Quarterly report</h1><p>Revenue: 1000</p>")
    end
end)

-- A never-cache endpoint for volatile data.
app:get("/now", function(ctx)
    ctx:cache({ noStore = true }):json({ time = os.time() })
end)

app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
})
```

### The ergonomic client

```lua
-- The ergonomic http client where get/post return a parsed response with status, ok, headers, body and json().
local async = require("async")

async.run(function()
    local http = require("http")

    -- A get with a query table appends a proper query string and parses the json response on demand.
    local base = os.getenv("VARN_HTTP_URL") or "https://httpbin.org"
    local resp = http.client.get(base .. "/get", { query = { name = "varn", lang = "en" } }):await()
    print("get status", resp.status, "ok", resp.ok)
    print("echoed name", resp.json().args.name)

    -- A post with a json option serializes the body and sets a Content-Type of application/json.
    local posted = http.client.post(base .. "/post", { json = { value = 42, tags = { "a", "b" } } }):await()
    print("post status", posted.status)
    print("server saw json", posted.json().json.value)
end)
```

### A client request

```lua
local async = require("async")

async.spawn(function()
    local http = require("http")
    local url = os.getenv("VARN_HTTP_URL") or "https://httpbin.org/get"
    local response, err = http.client.requestRaw({
        url = url,
        method = "GET",
        headers = {},
        timeoutSeconds = 30
    }):await()
    if err then
        error(err)
    end
    print("status", response.status)
    print("body", response.body)
end)
```

### Server transport options

```lua
-- Creates an HTTP server and binds it to a local port using explicit transport options.
local http = require("http")

local port = 8080

http.createServer(function(_, res)
    res:finish("ok")
end):listen({ host = "127.0.0.1", port = port })

print("http server listening on http://127.0.0.1:" .. port)
```

### HTTPS JSON server

```lua
-- Serves json over tls using files in the working directory or varn tls env variables.
local http = require("http")

local server = http.createServer(function(req, res)
    res:json({
        ok = true,
        scheme = "https",
        host = req.host,
        path = req.path
    })
end)

server:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3443"),
    tls = true,
    certFile = os.getenv("VARN_TLS_CERT") or "cert.pem",
    keyFile = os.getenv("VARN_TLS_KEY") or "key.pem",
    publicDir = "apps/lua/public",
    servePublic = true
})
```

### Integrating the other modules

```lua
-- Combines async file reads hashing and static responses on one host from the repo root.
local http = require("http")
local async = require("async")
local fs = require("fs")
local crypto = require("crypto")

local dataDir = "build/_integration_tmp"

local function route(req, res)
    if req.path == "/write" then
        fs.writeFile(dataDir .. "/hello.txt", "Created by Varn."):await()
        res:finish("written")
        return
    end

    if req.path == "/read" then
        local value, err = fs.readFile(dataDir .. "/hello.txt"):await()
        if err then
            res:status(404)
            res:finish(err)
            return
        end
        res:finish(value)
        return
    end

    if req.path == "/hash" then
        res:finish(crypto.digest("SHA256", req.query.value or "varn", "hex"))
        return
    end

    if req.path == "/sleep" then
        async.sleep(3000):await()
        res:finish("done")
        return
    end

    res:finish("Varn modules example.")
end

http.createServer(route):listen(tonumber(os.getenv("VARN_PORT") or "3000"))
```

### JSON server

```lua
-- Serves small json payloads plus static files from the public tree.
local http = require("http")

local server = http.createServer(function(req, res)
    local name = req.query.name or "Nobody"

    res:json({
        ok = true,
        scheme = "http",
        host = req.host,
        path = req.path,
        name = name
    })
end)

server:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    servePublic = true
})
```

### JWT

```lua
-- Demonstrates issuing and verifying json web tokens without a server, then exits cleanly.
local http = require("http")

local secret = "topsecret"

-- Sign a short-lived token carrying a subject and a role claim.
local token = http.jwt.sign({ sub = "u1", role = "admin" }, secret, { expiresIn = 3600 })
print("token issued, length " .. #token)

-- Verifying with the right secret returns the decoded claims.
local claims, err = http.jwt.verify(token, secret)
assert(claims, err)
print("verified sub=" .. claims.sub .. " role=" .. claims.role)

-- Verifying with the wrong secret returns nil and an error, never the claims.
local forged, forgedErr = http.jwt.verify(token, "not-the-secret")
print("wrong secret rejected: " .. tostring(forged == nil) .. " (" .. tostring(forgedErr) .. ")")
```

### Server demo

```lua
-- Shows hello echo and hashed file routes in one process.
local http = require("http")
local async = require("async")
local fs = require("fs")
local crypto = require("crypto")

local server = http.createServer(function(req, res)
    if req.path == "/api/hello" then
        async.sleep(10):await()

        local name = req.query.name or "World"
        res:json({
            message = "Hello " .. name,
            host = req.host,
            method = req.method,
            remoteAddress = req.remoteAddress
        })
        return
    end

    if req.path == "/api/echo" then
        res:json({
            method = req.method,
            body = req.body,
            contentType = req.headers["Content-Type"] or req.headers["content-type"] or ""
        })
        return
    end

    if req.path == "/api/file" then
        local content, err = fs.readFile("apps/lua/public/index.html"):await()
        if err then
            res:status(500)
            res:finish(err)
            return
        end

        res:json({
            size = #content,
            sha256 = crypto.digest("SHA256", content, "hex")
        })
        return
    end

    res:status(404)
    res:finish("not found")
end)

server:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    servePublic = true
})
```

### Server-sent events

```lua
-- Server-sent events plus gzip with a live clock stream and a large json endpoint the server compresses automatically.
local http = require("http")
local async = require("async")

local app = http.createApp()

-- The /clock route streams sse events a browser reads through EventSource.
app:get("/clock", function(ctx)
    local stream = ctx:sse()
    for i = 1, 10 do
        stream:send("tick", os.date("%H:%M:%S"))
        stream:comment("keep-alive")
        async.sleep(1000):await()
        local _ = i
    end
    stream:close()
end)

-- A large json body is gzipped when the client sends an Accept-Encoding of gzip.
app:get("/data", function(ctx)
    local rows = {}
    for i = 1, 500 do
        rows[i] = { id = i, name = "row-" .. i, note = "a reasonably long descriptive label" }
    end
    ctx:json({ rows = rows })
end)

app:get("/", function(ctx)
    ctx:html([[
<!doctype html>
<h1>SSE clock</h1>
<pre id="out"></pre>
<script>
const out = document.getElementById("out");
const es = new EventSource("/clock");
es.addEventListener("tick", e => out.textContent += e.data + "\n");
</script>
]])
end)

-- Gzip is on by default and compress = false on listen turns it off.
app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    compress = true,
})
```

### URL encoding

```lua
-- Percent-encodes a value for a query string and decodes it back, available in every build including the browser.
local http = require("http")

local encoded = http.urlEncode("hello world & more")
print("encoded " .. encoded)

local decoded = http.urlDecode(encoded)
print("decoded " .. decoded)

assert(decoded == "hello world & more", "round-trip mismatch")

print("http url encode ok")
```

### WebSocket chat

```lua
-- Websocket chat with rooms where each client joins a room and messages fan out to that room's members.
local http = require("http")

local app = http.createApp()

-- A connection joins the room named in its query string, defaulting to "lobby".
app:ws("/chat", {
    open = function(conn)
        conn:join("lobby")
        conn:send("welcome to lobby")
    end,
    message = function(conn, data)
        -- A "/join <room>" command moves the sender while anything else broadcasts to the lobby.
        local room = data:match("^/join%s+(%S+)$")
        if room then
            conn:leave("lobby")
            conn:join(room)
            conn:send("joined " .. room)
            return
        end

        app:wsBroadcastRoom("lobby", data)
    end,
    close = function()
        print("a chat client disconnected")
    end,
})

-- A notifications endpoint pushes to everyone on the path with app:wsBroadcast.
app:ws("/notifications", {
    open = function(conn) conn:send("subscribed") end,
})

app:get("/announce", function(ctx)
    local count = app:wsBroadcast("/notifications", ctx.query.text or "ping")
    ctx:json({ delivered = count })
end)

app:get("/", function(ctx)
    ctx:html([[
<!doctype html>
<h1>WebSocket chat</h1>
<input id="msg"><button onclick="send()">send</button>
<pre id="log"></pre>
<script>
const ws = new WebSocket("ws://" + location.host + "/chat");
const log = document.getElementById("log");
ws.onmessage = e => log.textContent += e.data + "\n";
function send() { ws.send(document.getElementById("msg").value); }
</script>
]])
end)

app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
})
```

### XML server

```lua
-- Serves small xml payloads plus static files from the public tree.
local http = require("http")

local server = http.createServer(function(req, res)
    local name = req.query.name or "Nobody"

    res:xml({
        ok = "true",
        scheme = "http",
        host = req.host,
        path = req.path,
        name = name
    })
end)

server:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    servePublic = true
})
```
## Under the hood

The server runs an event loop on the same thread as Lua — `epoll` on Linux, `kqueue` on macOS/BSD, `IOCP` on Windows — so one process serves many thousands of connections without a thread per connection. Poco provides the sockets and TLS.

The client picks the transport its platform is best served by, without changing this API. On desktop it is built on Poco, in the browser it uses the host's `fetch`, on iOS it runs on `NSURLSession` and on Android on `HttpURLConnection`. An application's `Info.plist` and `network_security_config.xml` therefore govern trust anchors, certificate pinning and the cleartext policy, and the trust store, system proxy and HTTP/2 come from the operating system. Every transport hands a redirect to the caller rather than following it, and a compressed body is decoded before it is returned.

Each handler runs inline on the loop thread the moment its request is parsed, with no per-request hand-off, and the Lua runtime collects garbage generationally so the short-lived objects a request creates are reclaimed cheaply. The request object passed to a `createServer` handler materializes its fields through a metatable, so a handler that reads only `req.path` never pays to build the headers, cookies, or query. On plaintext connections static files are sent with the kernel's `sendfile`, going straight from the file to the socket without a copy through user space — over TLS the payload must be encrypted in user space, so it streams through the normal buffer.
