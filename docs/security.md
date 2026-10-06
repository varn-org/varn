# 🔒 Safety notes

Known trust boundaries and sharp edges for people running or embedding Varn. This is not a full audit.

## 🔐 crypto

The functions `crypto.digest` and `crypto.hmac` are convenience primitives, not side-channel-hardened building blocks. When you verify a MAC or token, compare it with `crypto.equals` (constant-time) rather than `==`, which leaks bytes through timing. For high-assurance work, handle it at the edge, such as TLS termination or an HSM. Do not pass untrusted strings as algorithm names without validating them yourself.

## 📁 fs

Like Node, `fs` is unrestricted by design. The functions `fs.readFile`/`fs.writeFile` operate on whatever path the script passes, with no sandbox, root confinement, or size cap — Varn runs your own code on your own machine. The consequence is that a path built from *untrusted request data* can read or overwrite anywhere the process can (traversal, symlink following). Validate or confine such paths in your own application (the static file handler below is already confined). Reading a never-ending special file (e.g. `/dev/zero`) will grow until memory runs out, exactly as in Node.

## 🗜️ zip

The function `zip.extract` rejects `..`, absolute entries, and paths that escape the destination (zip slip). Still review archives from untrusted sources. Total archive size and entry count are not capped today, so host disk quotas still apply.

## 🖥️ platform

The fields `platform.os` and `platform.arch`, and the filename helpers, expose non-secret host facts. They are not a substitute for secure packaging or attestation. The library-name/subdir helpers build paths from their arguments without sanitization and are typically fed into `ffi.load`, so the same "treat as native code" trust applies — do not derive the name or directory from untrusted input.

## 📝 log

Log messages are written verbatim, the same way `console.log` and `print` are. Control bytes are not escaped. If you log untrusted input, sequences like CRLF or terminal escapes pass through to the sink, which can forge log lines or drive a terminal. Sanitize untrusted data before logging when the sink is a shared log file or an interactive terminal.

## 🌐 http and http.client

Server-side code that calls `http.client.request` with user-controlled URLs can reach internal networks (SSRF). Restrict URLs in your application or with network egress rules. In the browser, `http.client.request` follows the browser's CORS, cookies, and redirects.

The method `ctx:file(path)` serves whatever path you give it with no root confinement — never build that path from untrusted request data without validating it (same traversal risk as `fs`). The built-in static file handler (`servePublic`) is confined and is the safe way to serve user-reachable paths.

The rate-limit middleware keys buckets by client IP. With `trustProxy = true` it reads the rightmost `X-Forwarded-For` entry, the address that the proxy in front of the server appended, since every entry before it arrived from the client and can be forged. Enable it only behind exactly one proxy that appends that entry. Without a proxy a client writes the whole header itself, so the limit is bypassable, and behind a chain of proxies the rightmost entry names the proxy before the last one, so every client shares its bucket.

At the transport level the server rejects ambiguous or abusive requests automatically: duplicate or conflicting `Content-Length`/`Transfer-Encoding` (request smuggling), bodies over `maxRequestBodyBytes` (16 MiB by default), which it answers with `413` as soon as the head declares them, and malformed chunked encoding. A chunked body keeps none of its framing in memory, so chunk extensions cannot grow the server past the body limit. A request pipelined behind one in flight is read only up to a bound and answered once the one before it is, so it is never answered twice or lost behind a streamed response. The server writes `Content-Length`, `Transfer-Encoding` and `Connection` itself, and a handler cannot set the last two. Connections that stall — a head that has not arrived whole in time (slowloris), a body that stops arriving, or a client reading its response too slowly — are closed once they pass `keepAliveTimeoutSeconds`, while a body that keeps arriving is never cut.

A build without TLS, such as the `port` target unless its host turns TLS on, refuses the `listen` option `tls` and the variables `VARN_TLS_CERT` and `VARN_TLS_KEY`, so a server asked for HTTPS never answers in plaintext with cookies marked `Secure` and an HSTS header. The scheme and the static files belong to each server of an application, so one application that listens over HTTPS and over plain HTTP marks only the cookies of its HTTPS server `Secure`.

An `app:ws` route accepts upgrades from any origin unless you set its `origins` allowlist. A WebSocket carries the browser's ambient cookies, so a cookie-authenticated socket without `origins` is open to cross-site WebSocket hijacking — set `origins` on any authenticated socket, the same way you would restrict CORS.

## 🧩 ffi

The module `ffi` can call any native function available to the process. Treat scripts that use `ffi` as equivalent to native code for sandboxing purposes.

## 🛡️ Reporting

Use the repository issue templates for vulnerabilities that should be handled privately. Follow any security policy published on the project's GitHub org.
