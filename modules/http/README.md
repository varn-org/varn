# 🌐 http

An in-process HTTP/1.1 server with a higher-level app framework (routing, middleware, sessions,
cookies, WebSockets, SSE, security middleware), an HTTP client, and URL percent-encoding helpers.

```lua
local http = require("http")
http.createServer(function(req, res)
    res:json({ hello = req.query.name or "world" })
end):listen(3000)
```

## Capabilities

| Function | What it does |
|---|---|
| `http.createServer(handler)` | A bare HTTP/1.1 server; the `(req, res)` handler runs once per request. `builder:listen(port|options)` starts it. |
| `http.createApp()` | An application with routing, groups, middleware, named routes, constraints, sessions, cookies, body parsing, file responses, WebSockets, and SSE. |
| `http.urlEncode(text)` | Percent-encodes `text` for a URL; every byte outside `A-Za-z0-9-_.~` becomes `%XX`. Binary-safe. |
| `http.urlDecode(text)` | Reverses it, turning `%XX` back into bytes and `+` into a space, so it also decodes form data. Binary-safe. |
| `http.cors(opts)` | CORS middleware; `origin` may be `"*"` or an allowlist table, with `methods` and `credentials` options. |
| `http.securityHeaders(opts)` | Sets `frameOptions`, `referrerPolicy`, `hsts`, and related security response headers. |
| `http.apiKey(opts)` | Gates routes behind an API key read from a `header`, compared in constant time against `keys`. |
| `http.rateLimit(opts)` | Throttles requests to `max` per `windowMs`, emitting throttle headers and `429` past the cap; tracks at most `maxClients` distinct addresses so the store stays bounded. |
| `http.csrf(opts)` | Double-submit CSRF protection; issues a session-bound token and rejects unsafe requests that lack a match. |
| `http.jwtAuth(opts)` | Verifies a `Bearer` token against `secret` and puts the decoded claims on `ctx.state.user`. |
| `http.requireAuth()` | Middleware that answers `401` unless `ctx.state.user` is set by a prior auth step. |
| `http.requireRole(role)` | Middleware that answers `401`/`403` unless the authenticated user carries `role`. |
| `http.jwt.sign(claims, secret, opts?)` | Signs a JWT (HS256) carrying `claims`; `opts.expiresIn` sets the `exp` claim. |
| `http.jwt.verify(token, secret)` | Verifies a JWT and returns its claims, or `nil` plus an error on a bad signature, expiry, or tamper. |
| `http.client.request(options)` | Sends a request from one options table (`url`, `method`, `headers`, `body`, `query`, `json`, …) and resolves to a response. |
| `http.client.get(url, options?)` | Convenience GET; resolves to a response with `status`, `ok`, `headers`, `body`, and `json()`. |
| `http.client.post(url, options?)` | Convenience POST; the `json` option serializes the body and sets `Content-Type: application/json`. |
| `http.client.requestRaw(options)` | Low-level call resolving to a `{ status, headers, body }` table that the higher-level helpers wrap with `ok` and `json()`. |
| `http.client.stream(opts, onChunk)` | Streams a response body, invoking `onChunk` per chunk and the optional `opts.onResponse(status, headers)` once the head arrives. |

## Availability

The **server** and **WebSockets** are native-only on every desktop and mobile platform — a browser
page cannot host a listener, so they are unavailable in wasm. The **client** works everywhere,
including the browser, where it runs over the host's `fetch`. The **URL encode/decode** helpers work
in every build, browser included. See the [platform matrix](../../docs/platform-availability.md).

## Reference, examples, and tests

- Full reference: [docs/lua-api/http.md](../../docs/lua-api/http.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
