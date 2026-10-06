# 🌐 http

An in-process HTTP/1.1 server (with a higher-level app framework), an HTTP client, and WebSocket support. JSON and XML response helpers are available when those modules are built.

## Server

```lua
local http = require("http")

http.createServer(function(req, res)
    res:json({ hello = req.query.name or "world" })
end):listen(3000)
```

The function `http.createServer(handler)` returns a builder. The handler runs once per request with `(req, res)`, as a task that may `:await()`.

The `req` fields are `host`, `method`, `path`, `target`, `queryString`, `body`, `remoteAddress`, `headers`, `cookies`, and `query` (the parsed query string as a table).

The `res` methods:

- `res:status(code)` — Set the response status.
- `res:setHeader(name, value)` — Set a header.
- `res:write(data)` — Stream `data` as the next part of the body and answer a promise, as described in [Streaming a response](#streaming-a-response).
- `res:finish(body?)` — Send an optional body and end the response.
- `res:json(table)` — Send a table as JSON.
- `res:xml(table)` — Send a table as XML.

If the handler returns without ending the response, at once or after an await, the server sends `204 No Content`, or ends the stream when it already wrote to it. A handler that fails, at once or after an await, ends a response it left open with `500`, or cuts a stream it started short by closing the connection without its end, and its failure reaches the handler of `async.onFailure` with its traceback and its frames, or the log when no handler is set, while the server goes on.

Calling `builder:listen(port)` or `builder:listen(options)` starts the server and answers the listening [`Server`](#the-listening-server). The same options work for both `http.createServer` and `app:listen`:

- `host` (default `"0.0.0.0"`) and `port` (default `3000`), where port `0` takes a free port that `server.port` then tells.
- `publicDir`, the folder of static files, and `servePublic`, which naming `publicDir` turns on unless it is set to `false`. No folder is served unless the options name one, so `servePublic = true` without `publicDir` is refused.
- `directoryListing` (default `false`) lists a folder of `publicDir` that has no index.
- `requestTimeoutMs` (default `30000`, `0` turns it off), the time a handler has to start its answer before the server answers `504`.
- `maxRequestBodyBytes` (default 16 MiB), the largest body the server reads, past which it answers `413`.
- `keepAliveTimeoutSeconds` (default `30`), `maxQueued` (the accept backlog) and `compress` (gzip responses, default `true`).
- `reusePort` (default `false`, and `true` in the worker processes of `VARN_WORKERS`) lets other sockets that ask for it share the port.
- `tls`, `certFile` and `keyFile`.

A port outside `0` to `65535`, a value of the wrong type and an option the server does not know are refused with a message that names the option, such as `[HttpServer] The listen option "prot" is unknown.`, so a misspelled option never passes silently. The environment variables `VARN_PORT`, `VARN_TLS_CERT` and `VARN_TLS_KEY` set the port and the certificate that the call does not name.

### The listening server

The value `listen` answers describes the server and closes it:

- `server.host` — The host the server listens on.
- `server.port` — The port it bound, which is the free port the system chose when the options asked for port `0`.
- `server:close(options?)` — Close the server and answer a promise that resolves to `true` once the listener and every connection are closed. The option `graceful` (default `false`) lets the requests in flight finish first, and `timeoutMs` (default `5000`) bounds that wait, after which what is left closes at once. An unknown option is refused by name.

A close stops listening at once, so the port is free and a new connection is refused. Without `graceful` every connection closes now and a response still in flight is cut. With it, an idle connection closes now, a request in flight gets its answer and its connection closes behind it, and no new request is taken, even on a connection the close lets finish. Either way each WebSocket is closed with code `1001` (going away). Once closed the server no longer keeps the runtime alive, so a script whose only work was the server ends. Calling `close` again answers a promise that resolves once the first close is done.

Collecting the `Server` value closes nothing, so a script that keeps no reference still serves until it calls `close` or the runtime stops.

A server binds its port alone. A second server on a busy address fails with a message such as `[HttpServer] The address 127.0.0.1:41000 is already in use.` instead of quietly taking part of the connections of the first. On POSIX the server sets `SO_REUSEADDR`, so a restarted server rebinds a port still in `TIME_WAIT`, and on Windows it binds with `SO_EXCLUSIVEADDRUSE`. The option `reusePort` sets `SO_REUSEPORT` on POSIX and `SO_REUSEADDR` on Windows, so several servers that all ask for it share the port, which is how the worker processes of `VARN_WORKERS` share theirs.

## App framework

The function `http.createApp()` returns an application with routing, route groups, middleware, named routes, path constraints, sessions, cookies, body parsing, file responses, WebSockets, and built-in security middleware (`http.cors`, `http.securityHeaders`, `http.apiKey`, `http.rateLimit`, `http.csrf`, `http.jwtAuth`, `http.requireAuth`, `http.requireRole`, and `http.jwt.sign` / `http.jwt.verify`). The full tour is in the `app_full` example below.

A route, its middleware and its hooks run as a task that may `:await()`. A route that fails, at once or after an await, runs the handler of `app:onError(handler)` with the context and the text of the error, answers `500` when the response is still open or cuts a stream it started short, and its failure reaches the handler of `async.onFailure` with its traceback and its frames, or the log when no handler is set.

The application carries an event bus so a handler can announce something without knowing who reacts to it. The method `app:on(name, handler)` subscribes and `app:emit(name, ...)` publishes, passing every extra argument through to each handler in subscription order. Delivery is synchronous and in-process, the handler list is copied before any of it runs so a handler may subscribe another one, and a handler that raises is logged and skipped rather than failing the request that emitted the event. The bus is private to one worker, so a process started with several workers does not share events between them.

The middleware `http.rateLimit(opts)` takes `windowMs` (default `60000`), `max` requests per window (default `100`), `trustProxy` to read the client address from `X-Forwarded-For` (default `false`), and `maxClients`, the number of distinct client addresses tracked (default `100000`). The store is bounded on purpose: one IPv6 client can source from an entire prefix, so once `maxClients` is reached the limiter drops expired entries and, if it is still full, starts a fresh window for everyone rather than growing without end. Size it to the traffic you expect to serve, not to the traffic you expect to be attacked with.

### Context response helpers

Inside a handler `ctx` carries the request as `ctx.req` (equivalently `ctx.request`) plus the shorthands `ctx.method`, `ctx.path`, `ctx.params`, `ctx.query` and `ctx.state`, and it ends the response through `json`/`xml`/`text`/`html`/`file`/`status`/`header`/`cookie`/`type`/`redirect`/`write`/`send` (`finish` is accepted as the same call as `send`, so a handler body reads the same on `createServer` and on `createApp`). The method `ctx:xml(table)` is the XML twin of `ctx:json(table)`, sending `application/xml; charset=utf-8`. On top of those:

- `ctx:cache(seconds)` or `ctx:cache(opts)` — Set `Cache-Control`. A number is shorthand for `public, max-age=<n>`. The options table understands `maxAge`, `sMaxAge`, `private` (default is `public`), `noStore`, `noCache`, and `mustRevalidate`. Returns `ctx` for chaining.
- `ctx:etag(value)` — Set the `ETag` header (a bare value is quoted, a `W/` prefix is kept weak). When the request's `If-None-Match` matches, it answers `304` and ends the response, so guard the rest of the handler with `if ctx.req.headers["If-None-Match"] then return end`.
- `ctx:file(path, opts?)` — Stream a file, with the content type taken from its extension. A path that is not an existing regular file answers `404`, so a directory, a fifo or a device is never streamed. Setting `opts.download = true` sends it as an attachment named after the file, and `opts.download = "name.ext"` names it explicitly. Either way the name is encoded per RFC 6266, so a filename cannot break out of the header. **The path is served exactly as given** — this is the explicit escape hatch, not the guarded static handler, so a handler that builds the path from request data must contain it itself. Serving a whole directory belongs to `publicDir`, which resolves and confines every path for you.
- `ctx:accepts(type1, type2, ...)` — Return the best match against the request `Accept` header, or `nil` if none fit. A bare token like `"json"` or `"html"` matches the media subtype. A full type like `"application/json"` matches exactly. A missing or `*/*` Accept header returns the first type.
- `ctx:sse()` — Switch the response to `text/event-stream` with `Cache-Control: no-cache` and return a Server-Sent-Events writer:
  - `stream:send(data)` — Send a default-event message. The payload may hold anything: a line break of any kind (CR, LF or CRLF, all three of which end a line in the protocol) is escaped into its own `data:` field, so text can never break out of the record.
  - `stream:send(event, data)` — Send a named event.
  - `stream:comment(text)` — Send a comment line, the conventional heartbeat that keeps proxies open.
  - `stream:close()` — End the stream.

  An **event name** and a **comment** each occupy a whole line of the stream, so neither may contain a line break — one would let the rest of the value forge `id:`, `retry:` or an entire further event on the client. Passing one raises, the way an invalid header name does, rather than being silently stripped. Payload data has no such restriction, since it is escaped.

  The writer builds on the streamed writes below, so frames flush progressively rather than buffering.

### Streaming a response

The method `ctx:write(data)`, and `res:write(data)` on a server of `http.createServer`, streams the body as the handler writes it. The first write sends the head with the status and headers set so far, which no later call changes, and every write goes out as soon as the client takes it.

- A response whose handler set a `Content-Length` through `ctx:header` or `res:setHeader` is framed by that length. A write that would pass it rejects with a message naming `Content-Length` and sends nothing of itself, and a stream that ends short of it closes the connection, so the client never takes a short body for a whole one.
- Any other response uses chunked transfer encoding, and each non-empty write is one chunk.
- The method `ctx:send(body?)` (or `res:finish(body?)`) sends its body as the last write and ends the stream, and so does the end of the handler. A handler that fails mid-stream closes the connection after the data it wrote, without the end of the stream, so the client sees the body cut short instead of a whole one. A `HEAD` request and the statuses `204` and `304` send the head alone.

Each write answers a promise that resolves once its data left for the client and rejects once the client is gone, or when the response already ended. A handler that awaits every write therefore never holds more than the client takes, and a client that stops reading holds the handler at its next write. Writes that nobody awaits still go out in order, and a connection whose unsent data passes 16 MB while the client reads nothing is dropped, so writes that ignore their promises cannot grow the server without bound.

```lua
local async = require("async")
local http = require("http")

local app = http.createApp()

app:get("/report", function(ctx)
    ctx:type("text/plain")
    for line = 1, 1000 do
        local ok, err = ctx:write("line " .. line .. "\n"):await()
        if not ok then
            print("The client left: " .. err)
            return
        end
    end
    ctx:send("done\n")
end)

app:listen({ port = 3000 })
```

The body of `ctx:send`, `ctx:json` and the other helpers that end a response goes out as its own buffer behind the head, so a large body is copied once from Lua and never again.

### Response compression

When the request carries `Accept-Encoding: gzip` and the response body is a compressible type (`application/json`, `application/xml`, or any `text/*`) above 1 KB, the server gzips the body and sets `Content-Encoding: gzip` plus `Vary: Accept-Encoding`. Already-encoded or tiny bodies are left alone, and a streamed response goes out as it is written. Compression is on by default. Pass `compress = false` to `listen` to disable it.

### WebSockets

The method `app:ws(path, handlers, options?)` serves WebSockets on a path. The table `handlers` holds the callbacks and the allowlist of origins:

- `open(conn)` — A client connected.
- `message(conn, data, binary)` — A whole message arrived, with `binary` set to `true` for a binary message and `false` for text.
- `close(conn, code, reason)` — The connection closed, with the code and the reason of the close frame that ended it (see below).
- `pong(conn, payload)` — The client answered a ping.
- `origins` — A list of the `Origin` values allowed to connect, so a page of another site cannot open the socket. An upgrade from any other origin is refused with `403`.

The table `options` sets the limits of the route, and an unknown option or a value out of range is refused by name:

- `maxMessageBytes` (default 16 MiB) — The largest message the server takes, whole or in fragments. A larger one closes the connection with code `1009`.
- `maxQueuedBytes` (default 16 MiB) — The most the server keeps queued for a client that does not read. It counts the bytes of the messages and never the heads of their frames, so a message of exactly the limit still goes out on an empty queue. A send that would pass it drops the connection.
- `pingIntervalMs` (default `0`, off) — Ping the client at this interval and drop one that did not answer the previous ping, which finds a client that vanished without closing.
- `protocols` — A list of subprotocols the route speaks, in its order of preference. The server answers with the first of them the client offers in `Sec-WebSocket-Protocol`, and refuses with `400` a client that offers none of them.

Each connection has these methods and fields:

- `conn:send(data, options?)` — Queue a message and answer a promise that resolves to `true` once it left for the client and to `false` when the connection closed before it did. The option `binary = true` sends a binary frame, and a message is text by default. A handler that awaits each send never holds more than the client takes. The promise never rejects, so a send that nobody awaits never becomes an unobserved rejection.
- `conn:close(code?, reason?)` — Close with `code` (default `1000`) and an optional `reason` of at most 123 bytes of UTF-8. A code the protocol reserves, such as `1005` or `1006`, is refused, and so are codes outside `1000` to `4999` that no one assigned.
- `conn:ping(payload?)` — Send a ping with a payload of at most 125 bytes, which the `pong` callback hears back.
- `conn:join(room)` and `conn:leave(room)` — Manage the rooms of the connection.
- `conn.bufferedAmount` — How many bytes of messages wait in the queue for the client.
- `conn.protocol` — The subprotocol the route chose, or `nil`.

A text message must be UTF-8 both ways. A text frame from the client that is not UTF-8 closes the connection with code `1007`, as browsers do, while a binary message carries any bytes. The close callback receives the code of the close frame that ended the connection: the code the client sent, `1005` when its close frame carried none, the code the server sent with `conn:close` or for a frame it refused (`1002` for a protocol error, `1007` and `1009` as above, `1001` when the server closes), and `1006` with a reason when the connection was lost without a close frame, such as a client that stopped reading past `maxQueuedBytes` or did not answer a ping.

Live connections are tracked per app so a handler can reach the others:

- `app:wsBroadcast(path, data, options?)` — Send `data` to every open connection on that WebSocket path and return how many it reached. The option `binary = true` sends a binary frame. The message is copied once and shared by every connection.
- `app:wsBroadcastRoom(room, data, options?)` — Send `data` to every connection in that room, with the same options, and return how many it reached.

Connections register on open and are removed on close, so broadcasts never touch a dead socket.

The callbacks `open`, `message`, `pong` and `close` each run as a task that may `:await()`. A callback that fails, at once or after an await, hands its failure to the handler of `async.onFailure` with its traceback and its frames, or to the log when no handler is set, and the connection stays open.

### Routing patterns

A path is a list of `/`-separated segments. A segment is either a literal, a named param, or a wildcard:

- `:name` — A named param. Its value lands in `ctx.params.name`. Add a constraint with `:where("name", "int" | "alpha" | "alnum" | "slug" | "uuid" | <regex>)`.
- `:name?` — An **optional** param. The route matches whether or not that trailing segment is present. When absent, `ctx.params.name` is `nil`. A constraint still applies when the segment is present. Example: `/posts/:id?` matches both `/posts` and `/posts/42`.
- `*` — A **wildcard** (catch-all) terminal segment. It captures the remaining path, including any `/`, and exposes it as `ctx.params.wildcard`. An empty tail is allowed. Example: `/files/*` matches `/files`, `/files/a`, and `/files/a/b/c.txt` (`wildcard` is `"a/b/c.txt"`). Calling `app:url(name, { wildcard = "a/b" })` rebuilds the URL with the tail.

## Execution model

Lua runs on a single thread (its own `lua_State`). Blocking I/O is offloaded to a worker pool and results are marshalled back. Request handlers, middleware, and WebSocket callbacks therefore run **one at a time** on that thread — like Node's event loop. Use `:await()` for I/O so the loop stays free. A handler that busy-loops or makes a synchronous blocking call will stall every other connection until it returns. HTTP requests are bounded by `requestTimeoutMs`: a handler that has not started its answer by then is answered with `504` and runs on with a response that refuses whatever it sends later, while a response that already started to stream is never cut by it. The callbacks of the messages of one connection start in order, and a callback that awaits lets the next message start before it ends.

## Scaling across cores

One process runs one event loop, so it uses one CPU core. Set `VARN_WORKERS=N` to run `N` worker processes: a master forks them, each binds the same port with `reusePort` on, which sets `SO_REUSEPORT`, and the master restarts any worker that exits. This is the model Node's `cluster` and nginx use — on Linux the kernel load-balances new connections across the workers. On Windows, which has no `fork`, the master relaunches itself as the worker processes instead. The variable `VARN_WORKERS` defaults to `1` and is capped at `1024`. The tvOS, watchOS, and visionOS targets and the browser have no multi-process model, so the server stays single-process there.

**Workers share nothing.** Each one is a separate process with its own `lua_State`, so anything a handler keeps in memory is private to the worker that served the request. That includes the session store behind `ctx:session()`, the CSRF secret behind `http.csrf()`, and the counters behind `http.rateLimit()`. Because the kernel spreads a client's connections across workers, a session started on one worker is invisible to the next, a CSRF token issued by one worker is rejected by another with `403`, and a rate limit of `max` per window becomes `max × N` in the worst case. The server logs an error the first time `ctx:session()` runs with `VARN_WORKERS` above `1`. Run a single worker when you need those, or keep the state in a shared store — the [`redis`](https://github.com/varn-org/components/blob/main/docs/redis.md) component is the usual choice.

## Request hardening

The server fails closed on ambiguous or abusive requests: duplicate or conflicting `Content-Length`/`Transfer-Encoding` headers (request smuggling) are answered with `400`, and a malformed chunk size is rejected. A body over `maxRequestBodyBytes` gets `413` with `Connection: close` as soon as the head declares it, or as soon as a chunk would pass the limit, before the server reads any of it. The server then reads off at most 1 MiB of the rest for at most two seconds and closes, so an upload far too large never holds it, while a client that sends its whole body before it reads may see the connection reset instead of the answer. A connection that stops making progress — a slow or partial request, or a client reading its response too slowly (slowloris) — is closed once it passes `requestTimeoutMs`/`keepAliveTimeoutSeconds`.

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

The functions `http.client.request(options)`, `http.client.get(url, options?)` and `http.client.post(url, options?)` return a promise that resolves to a response table:

- `status` — The numeric HTTP status.
- `ok` — Set to `true` when `status < 400`.
- `headers` — A table of the response headers with lowercased keys.
- `body` — The raw response body string.
- `json()` — Parses `body` as JSON on demand (requires the `json` module).

Options: `url` (required for `request`), `method` (default `"GET"`), `headers` (table), `body` (string), `timeoutSeconds` (default `60`), `verifyTls` (default `true`), `insecure` (opt-out of TLS verification for dev certs), `maxResponseBytes` (default 64 MB), plus two ergonomic shortcuts:

- `query = { k = v }` — Appended to the URL as a sorted, percent-encoded query string.
- `json = value` — Serialized with the `json` module and sent with `Content-Type: application/json` (unless you set that header yourself).

The option `timeoutSeconds` is one deadline for the whole request, from the connection to the last byte of the body and across every redirect, so a server that keeps sending slowly still ends it on time. Any positive number of seconds works, a fraction included, and anything else refuses the request with a message that names the option. A request past its deadline rejects with a message saying it did not finish within its timeout. A secure request never waits for the server to answer the close of its TLS session once the response is whole.

On failure the promise rejects with a message.

For low-level access, `http.client.requestRaw(options)` resolves to a plain `{ status, headers, body }` table without the `ok` flag or the `json()` helper. The ergonomic surface above is a thin wrapper over it.

### Redirects

A response that points somewhere else is followed, which is what every client does unless it is told otherwise. Two options say what happens instead, and both work on every target because the following is done above the driver rather than inside each one.

| Option | Takes | Default |
|---|---|---|
| `redirect` | `"follow"`, `"manual"` or `"error"` | `"follow"` |
| `maxRedirects` | How many hops are allowed before the request fails | `20` |

```lua
-- Followed to wherever it ends up, which is what a picture service asks for.
local picture = http.client.requestRaw({ url = "https://picsum.photos/400/300" }):await()

-- Handed over untouched, for a caller that wants to read the Location itself.
local pointed = http.client.requestRaw({ url = address, redirect = "manual" }):await()

-- Treated as a failure, for a caller that meant to reach exactly what it named.
local exact, problem = http.client.requestRaw({ url = address, redirect = "error" }):await()
```

The rules are the ones the fetch standard settled on, since that is what most clients took. A `303` turns what was asked into a plain read of somewhere else, and so does a `301` or a `302` on anything but a `GET` or a `HEAD` — the method becomes `GET` and the body is dropped. A `307` and a `308` exist to say the opposite, so both keep the method and the body. A redirect to another origin drops `Authorization`, `Cookie` and `Proxy-Authorization`, since credentials belong to the host they were meant for. A redirect naming nowhere is the answer itself rather than a failure. Twenty is what the fetch standard allows, `requests` allows thirty and Go allows ten, so a caller who needs a different number says so.

The function `http.client.stream` follows the same way, and a redirect's own body is swallowed rather than handed to `onChunk`.

Under WebAssembly the browser follows a redirect itself and reports only where it ended up, so `redirect` there must be `"follow"`. Asking for `"manual"` or `"error"` is refused rather than answered with the final response as though it were the first.

To consume a response incrementally, `http.client.stream(options, onChunk)` invokes `onChunk` with each body chunk as it arrives (with `options.onResponse` called first with the status) and resolves once the response completes, which suits server-sent events and large downloads. A callback that fails reaches the handler of `async.onFailure` with its traceback and its frames, or the log when no handler is set, the stream calls none of its callbacks after it and stops reading the response, and the promise rejects with the text of that failure, never reported again as an unobserved rejection. A chunk is whatever the connection delivered, handed over at once rather than held until a buffer fills, so each event of a stream reaches `onChunk` when the server sends it. The function `http.client.streamRaw` is its lower-level form.

The head arrives before any piece of the body on every target, the browser included, where the body is read from the response as it arrives rather than once it ends. A body past `maxResponseBytes` stops being read at the piece that passes the limit, or before any of it when the server declares a longer one, and the promise rejects with a message that names the address and the limit. A stream whose callbacks fall behind holds only a bounded window of the body before the connection waits for them, so a slow consumer never gathers a whole download in memory.

### Resuming a download

A download that stopped resumes from what it already holds. The header `Range` asks for the rest, a server that honours it answers `206` with a `Content-Range` that says where the piece starts, and one that answers `200` sends the whole body again, so what was held is dropped.

```lua
local fs = require("fs")
local held = fs.exists(path) and fs.stat(path):await().size or 0
local partial = false

http.client.stream({
    url = "https://example.com/levels/forest.pak",
    headers = { Range = "bytes=" .. held .. "-" },
    timeoutSeconds = 600,
    maxResponseBytes = 2 * 1024 * 1024 * 1024,
    onResponse = function(status, headers)
        partial = status == 206 and (headers["content-range"] or ""):match("^bytes (%d+)%-") == tostring(held)
    end,
}, function(chunk)
    -- Append to the file when `partial` is set and write it from the start otherwise.
end):await()
```

Sending `If-Range` with the `ETag` of the first response makes a server that changed the file send it whole rather than a piece of the new one, and a `416` answers a range that starts at the end, which means the file is already whole.

## URL encoding

Percent-encoding helpers, available in every build including the browser:

- `http.urlEncode(text)` → Percent-encodes `text` for a URL. Every byte outside the RFC 3986 unreserved set (`A-Za-z0-9-_.~`) becomes `%XX`, so a space becomes `%20`. Use it for a query value or a path segment. Binary-safe.
- `http.urlDecode(text)` → Reverses it, turning `%XX` back into bytes and a `+` into a space, so it also decodes `application/x-www-form-urlencoded` data. Binary-safe.

```lua
local http = require("http")
local q = http.urlEncode("hello world & more")  -- "hello%20world%20%26%20more"
print(http.urlDecode(q))                         -- "hello world & more"
```

## Examples

### The full app tour

```lua
-- Full tour of the HTTP app framework covering routing, groups, middleware, params, constraints, named routes, cookies, sessions, body parsing, uploads, downloads and static files.
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

-- Path params plus a constraint and a named route for URL building.
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

-- Route groups share a prefix and their own middleware (also used for versioning), and this group enforces an API key and a rate limit on top of the custom token check.
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

-- Body parsing detects JSON, form-urlencoded and multipart from the content type.
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

-- JWT issue + verify, with a role guarded area.
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

-- CSRF double-submit protection for browser forms.
local forms = app:group("/forms")
forms:use(http.csrf())
forms:get("/token", function(ctx) ctx:json({ csrf = ctx.state.csrfToken }) end)
forms:post("/submit", function(ctx) ctx:json({ ok = true }) end)

-- Streaming sends each write as its own chunk with chunked transfer encoding, and awaiting a write waits until the client took it.
local async = require("async")
app:get("/stream", function(ctx)
    ctx:type("text/plain")
    for i = 1, 5 do
        ctx:write("chunk " .. i .. "\n"):await()
        async.sleep(100):await()
    end
    ctx:send()
end)

-- WebSocket endpoint with open/message/close callbacks (server owns the socket, Lua stays on its thread).
app:ws("/ws", {
    open = function(conn) conn:send("welcome") end,
    message = function(conn, data, binary)
        if data == "bye" then
            conn:send("closing")
            conn:close(1000, "bye")
        else
            conn:send(binary and data or "echo:" .. data, { binary = binary })
        end
    end,
    close = function(_, code, reason) print("websocket closed", code, reason) end,
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
local server = app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    directoryListing = true,
    requestTimeoutMs = 30000,
    maxRequestBodyBytes = 4 * 1024 * 1024,
})

print("The tour listens on port " .. server.port .. ".")
```

### App round trip

```lua
-- Drives an in-process app with the client to show routing, body parsing and a JSON response then exits, with the server on its own threads so the script issues its requests inside async.run and finishes cleanly.
local async = require("async")
local http = require("http")

local port = 8091
local base = "http://127.0.0.1:" .. port

local app = http.createApp()

-- A path parameter constrained to digits feeds a JSON response.
app:get("/users/:id", function(ctx)
    ctx:json({ id = ctx.params.id })
end):where("id", "int")

-- JSON body parsing detects the content type and hands back a parsed table.
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
-- Caching and content negotiation covering Cache-Control, ETag revalidation, and API-vs-HTML responses from one route.
local http = require("http")

local app = http.createApp()

-- A long-lived cached resource sets Cache-Control plus an ETag the client can revalidate against.
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

-- The same path serves HTML to a browser and JSON to an API client based on Accept.
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
-- The ergonomic HTTP client where get/post return a parsed response with status, ok, headers, body and json().
local async = require("async")

async.run(function()
    local http = require("http")

    -- A GET with a query table appends a proper query string and parses the JSON response on demand.
    local base = os.getenv("VARN_HTTP_URL") or "https://httpbin.org"
    local resp = http.client.get(base .. "/get", { query = { name = "varn", lang = "en" } }):await()
    print("get status", resp.status, "ok", resp.ok)
    print("echoed name", resp.json().args.name)

    -- A POST with a json option serializes the body and sets a Content-Type of application/json.
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
-- Creates an HTTP server on a free local port, tells where it listens and closes it once its requests finished.
local async = require("async")
local http = require("http")

local server = http.createServer(function(_, res)
    res:finish("ok")
end):listen({ host = "127.0.0.1", port = 0 })

print("http server listening on http://" .. server.host .. ":" .. server.port)

async.spawn(function()
    local response = http.client.get("http://127.0.0.1:" .. server.port .. "/"):await()
    print("status", response.status)

    -- A graceful close lets the requests in flight finish, and waits five seconds at most.
    server:close({ graceful = true, timeoutMs = 5000 }):await()
    print("server closed")
end)
```

### HTTPS JSON server

```lua
-- Serves JSON over TLS using files in the working directory or the Varn TLS env variables.
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
-- Serves small JSON payloads plus static files from the public tree.
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
-- Demonstrates issuing and verifying JSON Web Tokens without a server, then exits cleanly.
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
-- Server-sent events plus gzip with a live clock stream and a large JSON endpoint the server compresses automatically.
local http = require("http")
local async = require("async")

local app = http.createApp()

-- The /clock route streams SSE events a browser reads through EventSource.
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

-- A large JSON body is gzipped when the client sends an Accept-Encoding of gzip.
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
-- WebSocket chat with rooms where each client joins a room and messages fan out to that room's members.
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

### WebSocket binary messages, limits and backpressure

```lua
-- Echoes binary and text messages, pings idle clients, negotiates a subprotocol and streams a large reply without outrunning the client.
local http = require("http")

local app = http.createApp()

app:ws("/feed", {
    open = function(conn)
        print("connected with protocol", conn.protocol)
    end,
    message = function(conn, data, binary)
        if data == "dump" then
            -- Each awaited send waits until the message left, so the queue never holds more than one part.
            for part = 1, 100 do
                if not conn:send(string.rep("x", 64 * 1024), { binary = true }):await() then
                    return
                end
            end
            conn:send("done")
            return
        end

        if data == "quit" then
            conn:close(4000, "the client asked to leave")
            return
        end

        conn:send(data, { binary = binary })
        print("waiting for the client", conn.bufferedAmount)
    end,
    pong = function(_, payload)
        print("pong", #payload)
    end,
    close = function(_, code, reason)
        print("closed", code, reason)
    end,
}, {
    maxMessageBytes = 1024 * 1024,
    maxQueuedBytes = 4 * 1024 * 1024,
    pingIntervalMs = 30000,
    protocols = { "feed.v2", "feed.v1" },
})

app:get("/push", function(ctx)
    local count = app:wsBroadcast("/feed", string.char(1, 2, 3), { binary = true })
    ctx:json({ delivered = count })
end)

app:listen({ host = "127.0.0.1", port = 3000 })
```

### XML server

```lua
-- Serves small XML payloads plus static files from the public tree.
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

The client picks the transport its platform is best served by, without changing this API. On desktop it is built on Poco, in the browser it uses the host's `fetch`, on iOS it runs on `NSURLSession` and on Android on `HttpURLConnection`. An application's `Info.plist` and `network_security_config.xml` therefore govern trust anchors, certificate pinning and the cleartext policy, and the trust store, system proxy and HTTP/2 come from the operating system. Every transport hands a redirect to the caller rather than following it, and a compressed body is decoded before it is returned. A client keeps the connections it opened and reuses them for the next request to the same origin, a pool of idle connections on the desktop, one shared session on Apple platforms and the pool of the platform on Android and in the browser, so a request to a host already reached skips the TCP and TLS handshakes. In the browser the headers of a response are the ones its CORS rules expose to the page. The URL Loading System of Apple platforms holds the first bytes of a body whose type it may sniff, so a server that streams small pieces there sends the `Content-Type` `text/event-stream` or the header `X-Content-Type-Options: nosniff`.

An engine written in C++ reaches the same client through the class `varn::http::client::HttpClient`, which streams to callbacks without a Lua table in between and is described in [the embedding guide](../embedding.md#sending-http-requests-from-c).

Each handler runs inline on the loop thread the moment its request is parsed, with no per-request hand-off, and the Lua runtime collects garbage generationally so the short-lived objects a request creates are reclaimed cheaply. The request object passed to a `createServer` handler materializes its fields through a metatable, so a handler that reads only `req.path` never pays to build the headers, cookies, or query. On plaintext connections static files are sent with the kernel's `sendfile`, going straight from the file to the socket without a copy through user space — over TLS the payload must be encrypted in user space, so it streams through the normal buffer.
