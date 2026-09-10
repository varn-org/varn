# 🧭 Roadmap

A per-module gap analysis against what Express, Fastify, FastAPI, Django, and the Node/Python standard libraries give developers **day-to-day**. The bar is real-world apps and keeping Varn **simple to use** — the 20% of features reached for 80% of the time, not exhaustive parity. Open items are tagged **[Essential]** (real apps get painful without it), **[Important]** (commonly needed), **[Nice]** (real but lower frequency), with a rough size.

Keep this file honest. When a gap closes, move it into "Already covered" in the same change.

## Already covered — do not rebuild

- **Web (`http`) server**: Routing with param constraints, optional params and a `*` catch-all wildcard, named routes and reverse URLs, route groups and versioning, global/group/route middleware, shipped middleware (CORS, security headers, CSRF, rate-limit, API-key, JWT + role guards), sessions with rotation, body parsing (json/form/multipart + uploads), full cookie attributes, static files (ETag/range/304/cache + `sendfile`), `ctx:etag()` and `ctx:accepts()` on dynamic responses, gzip response compression negotiated on `Accept-Encoding`, `ctx:sse()` server-sent events, chunked streaming, WebSockets with broadcast, centralized error and 404 handlers, request hardening, multi-process scaling with `SO_REUSEPORT`.
- **Web (`http`) client**: One options table in, a `{ status, ok, headers, body, json() }` response out, plus `client.stream` delivering each piece of the body as it arrives with an `onResponse(status, headers)` head callback, and one deadline for the whole request that takes a fraction of a second.
- **Data**: The modules `json` and `xml`, the **`vdo`** component (PDO-style SQL for SQLite/MySQL/Postgres with prepared statements and transactions), the **`redis`** and **`mysql`** components, and the **`pool`** connection pool. The database and cache stories are filled by components, which keeps the C++ core lean.
- **System**: The modules `fs` (read/write/exists/mkdir/remove, streaming `open` handle, `stat`, `readdir`, `rename`, `copy`, `append`, `mkdtemp`), `socket` (TCP/UDP, TLS client sockets, Unix domain sockets), `process` (`exec` with captured stdout/stderr/exit code, `getenv`, `env`, `cwd`, `argv`), `zip`, `ffi` (calls through function pointers, casts of light userdata, pointers to const, arrays inside structs, enums, bitfields, `intptr_t`, callbacks that run on the main thread and report their failures, symbols a host adds, UTF-8 library paths on Windows), `platform`, `datetime`.
- **Crypto**: The functions `digest`, `hmac`, `randomBytes`, constant-time `equals`, `base64`/`base64url`/`hex` codecs, `uuidV4`/`uuidV7`, `hashPassword`/`verifyPassword`, AES-256-GCM `encrypt`/`decrypt`, `rsaEncryptPublic`, `pbkdf2` and `hkdf`.
- **Runtime**: The module `async` (`sleep`/`spawn`/`run`/`promise`/`deferred`, task handles that cancel and close the task, one handler for every task failure with its traceback and frames, and the `all`, `allSettled`, `race`, `any`, `timeout` and `mapLimit` combinators, which wait without polling and remove a deadline that was met), leveled `log` with runtime level config, structured key/value fields and a file or rotating sink.
- **Components**: The components `ai`, `scheduler`, `vdo`, `redis`, `mysql`, `pool`, `env`, `validate`, `test`, `retry`.

## Open gaps

### `fs` (native)
- **[Nice]** A `glob` / recursive directory walk. The function `readdir` lists one level, so walking a tree is Lua boilerplate today. *small*

### `process` (native)
- **[Nice]** A streaming `process.spawn` alongside the buffered `process.exec`, delivering stdout and stderr per chunk instead of one captured result. Long-running children (a build, a log tail) are the case `exec` cannot serve, since it resolves only when the child exits. *medium*
