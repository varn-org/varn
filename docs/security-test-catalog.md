# Varn Security Vulnerability & Test Catalog

An exhaustive, per-module catalog of security tests — bugs, pentest cases, OOM, races,
overflows, injection, fuzzing, and every known attack class (common and rare). Each module
carries **100+ test cases** so the whole battery can be run against Varn on every change.
It enumerates *what to attack* and *how it is triggered*; no remediation state is tracked.

Each module has its own page under [security-tests/](security-tests/). Start with the
cross-cutting baseline, then run each module's page plus those shared rows.

## Modules

| Module | Tests | Focus |
|--------|------:|-------|
| [Cross-cutting](security-tests/cross-cutting.md) | 302 | memory, integer, Lua binding, concurrency, OOM, encoding, errors, UB/sanitizers, side-channels, supply-chain, hardening |
| [http — server & app](security-tests/http-server.md) | 383 | JWT, sessions, authz, injection, smuggling/desync, cache poisoning, CSRF/CORS, routing, static, DoS, headers, TLS, WebSocket, business logic |
| [http — client](security-tests/http-client.md) | 300 | SSRF + bypasses, DNS rebinding, TLS verification, redirects, response/header injection |
| [json](security-tests/json.md) | 200 | type confusion, NaN/UTF-8, parser divergence/fuzz, numbers, canonicalization, round-trip |
| [xml](security-tests/xml.md) | 300 | XXE, entity expansion, DTD, XInclude/XSLT, signature wrapping, parser fuzz, encode injection |
| [crypto](security-tests/crypto.md) | 300 | weak algos, named attacks, timing, RNG CVEs, input caps, OpenSSL ctx leaks, key hygiene |
| [fs](security-tests/fs.md) | 300 | traversal, symlink/TOCTOU, arbitrary R/W, special files, OOM, binary safety |
| [ffi](security-tests/ffi.md) | 300 | parser, marshalling, ABI, type confusion, lib loading, callbacks, native memory, RCE surface |
| [socket](security-tests/socket.md) | 300 | SSRF, buffers, FD exhaustion, blocking, UDP spoofing/amplification, framing, hijacking |
| [zip](security-tests/zip.md) | 300 | zip slip, zip bomb/quine, symlink entries, format fuzz, create-path parity |
| [async](security-tests/async.md) | 200 | promise lifecycle, double-settle, cross-thread races, scheduling, starvation, cancellation |
| [core / runtime](security-tests/core.md) | 300 | Lua sandbox/bytecode, resource limits, event loop, teardown UAF, registry, env/args |
| [log](security-tests/log.md) | 200 | log/CRLF injection, pipeline (Log4Shell-class), terminal escapes, format string, disclosure, DoS |
| [platform](security-tests/platform.md) | 200 | info disclosure/fingerprinting, path building, arg validation, trust/supply-chain |

Total: **3885** test cases. Modules with a small attack surface (json, async, log, platform) stay at a
curated 200 to avoid filler; the rest are expanded to 300 with genuinely distinct, documented cases.

## Entry schema

```
ID       MODULE-AREA-NN
Name     the vulnerability / attack / test case
Class    CWE / OWASP (WSTG, ASVS, API Top 10) or RFC reference
Exploit  how it is triggered / the failure point / the input to send
```

## Methodology — axes applied to every entry point

- **Memory** — stack/heap overflow, OOB read/write, UAF, double-free, uninitialized, leak.
- **Integer** — overflow/underflow, signed↔unsigned, truncation on `size_t`/`int` casts.
- **Concurrency** — data race, TOCTOU, deadlock, reentrancy, main-loop vs worker.
- **Resource/OOM** — huge inputs, deep recursion, many objects, fd/thread/handle leaks.
- **Input** — empty, NUL, max/min, negative, Unicode, overlong UTF-8, control bytes, type mismatch.
- **Injection/parsing** — per format and per field, structured and dumb fuzzing.
- **Logic/auth** — authn/authz/crypto correctness where the module touches them.
- **Error handling** — fail-closed, no info leak, exception safety across the C boundary.

## How to run

- Per-module Lua tests: `./build/bin/varn modules/<m>/lua/tests/<m>_test.lua`.
- HTTP end-to-end: start a server script with `./build/bin/varn <script>.lua &`, then drive
  it with `curl` (the embedded `http.client` does not expose response headers/cookies — use
  `curl -c/-b jar -D -`).
- Native memory/UB: build and run under AddressSanitizer / UndefinedBehaviorSanitizer.
- Parsers (json, xml, zip, ffi cdef, http): run structured + dumb fuzzers over the entry points.
- Concurrency: stress with many parallel requests/connections and ThreadSanitizer.

## Frameworks covered

OWASP WSTG v4.2, OWASP ASVS, OWASP API Security Top 10 (2023), CWE Top 25 + memory-safety
classes (CWE-1399), SANS/CERT C++ guidance, and the per-domain attack literature.

## References

- OWASP WSTG v4.2 — https://owasp.org/www-project-web-security-testing-guide/v42/
- OWASP ASVS — https://owasp.org/www-project-application-security-verification-standard/
- OWASP API Security Top 10 (2023) — https://owasp.org/API-Security/editions/2023/en/
- OWASP Cheat Sheets — CSRF, Session Management, WebSocket, JWT, Cookie, REST, XXE Prevention.
- PortSwigger Web Security Academy — JWT, CORS, SSRF, request smuggling, WebSockets.
- PayloadsAllTheThings — Directory Traversal, CORS, SSRF, JWT, XXE.
- CWE Top 25 + CWE-1399 (memory safety) — https://cwe.mitre.org/
- HackTricks — JWT, HTTP request smuggling, SSRF, Lua sandbox escape, zip slip.
- RFCs — 9110/9112 (HTTP), 6265bis (Cookies), 7519/8725 (JWT/BCP), 6266 (Content-Disposition),
  5987 (header encoding), 7233 (Range), 9113 (HTTP/2), 4231 (HMAC test vectors).
