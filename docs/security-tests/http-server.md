# 🌐 http — server & app framework

### JWT & token auth

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-001 | `alg:none` (lowercase) | CWE-347 | A `{"alg":"none"}` header with empty signature accepted |
| HTTP-002 | `alg:None`/`NONE` case variants | CWE-347 | Case-insensitive `none` bypasses a naive check |
| HTTP-003 | Algorithm confusion RS256→HS256 | CWE-347 | Sign with the RSA public key as the HMAC secret |
| HTTP-004 | Algorithm confusion ES256→HS256 | CWE-347 | EC public key used as HMAC key |
| HTTP-005 | Algorithm downgrade HS512→HS256 | CWE-347 | Swap to a weaker accepted alg |
| HTTP-006 | Empty signature, valid alg | CWE-347 | Strip the signature but keep `alg:HS256` |
| HTTP-007 | Signature truncation | CWE-347 | Drop bytes from the MAC. Check rejects |
| HTTP-008 | Signature byte-flip | CWE-345 | Flip one MAC bit. Must reject |
| HTTP-009 | Payload tamper (role/sub) | CWE-345 | Edit claims without re-signing |
| HTTP-010 | Weak secret brute force | CWE-326 | Short/dictionary HMAC secret cracked offline |
| HTTP-011 | Empty secret accepted | CWE-347 | Verifying with `""` must never succeed |
| HTTP-012 | `jwk` header injection | CWE-347 | Attacker public key embedded in header |
| HTTP-013 | `jku`/`x5u` SSRF/key swap | CWE-918/347 | Key-set URL points at attacker host |
| HTTP-014 | `kid` path traversal | CWE-22 | Using `kid` as a file path loads attacker key |
| HTTP-015 | `kid` SQL injection | CWE-89 | The `kid` used in a key-lookup query |
| HTTP-016 | `kid` command injection | CWE-78 | The `kid` value reaches a shell during key load |
| HTTP-017 | `x5c` cert injection | CWE-347 | Self-signed cert trusted as anchor |
| HTTP-018 | `cty` nested-JWT abuse | CWE-20 | The `cty` header triggers nested parse/deserialization |
| HTTP-019 | `typ` confusion | CWE-20 | Unexpected `typ` accepted where it shouldn't |
| HTTP-020 | `crit` header ignored | RFC 8725 | Unknown critical extension not rejected |
| HTTP-021 | Expired token (`exp`) | CWE-613 | Past `exp` accepted |
| HTTP-022 | Missing `exp` | CWE-613 | Token never expires |
| HTTP-023 | `nbf` in the future | CWE-345 | Not-yet-valid token accepted |
| HTTP-024 | Clock-skew over-tolerance | CWE-613 | Excessive leeway accepts long-expired tokens |
| HTTP-025 | `exp`/`nbf` non-numeric | CWE-20 | String/object `exp` mis-handled |
| HTTP-026 | Audience not validated | CWE-287 | Token for service A accepted by B |
| HTTP-027 | Issuer not validated | CWE-287 | Untrusted `iss` accepted |
| HTTP-028 | Subject confusion | CWE-287 | Missing/duplicate `sub` handling |
| HTTP-029 | Non-object payload | CWE-20 | JSON-array payload indexed as claims |
| HTTP-030 | Token replay / no revocation | CWE-294 | Reuse after logout/compromise |
| HTTP-031 | Cross-JWT confusion | CWE-287 | Token from another app/realm accepted |
| HTTP-032 | base64url malleability | CWE-347 | Non-canonical base64 segments still verify |
| HTTP-033 | Unicode/whitespace in token | CWE-20 | Padded/whitespace token bypasses parsing |
| HTTP-034 | Sign mutates caller input | CWE-664 | Signing injects claims into the caller's object |
| HTTP-035 | Bearer parsing leniency | CWE-20 | A `bearer` scheme/extra spaces/empty token accepted |

### Sessions

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-036 | Predictable session ID | CWE-330 | Guessable/sequential ids |
| HTTP-037 | Insufficient ID entropy | CWE-331 | An id under 128 bits is brute-forceable |
| HTTP-038 | Session fixation | CWE-384 | Id not rotated on privilege change |
| HTTP-039 | Fixation via cookie injection | CWE-384 | Attacker-set id survives login |
| HTTP-040 | Missing `HttpOnly` | CWE-1004 | XSS reads the session cookie |
| HTTP-041 | Missing `Secure` (under TLS) | CWE-614 | Cookie sent over plaintext |
| HTTP-042 | Missing/weak `SameSite` | CWE-1275 | CSRF via cross-site cookie send |
| HTTP-043 | No idle timeout | CWE-613 | Abandoned session valid forever |
| HTTP-044 | No absolute timeout | CWE-613 | Active session never expires |
| HTTP-045 | Session store exhaustion | CWE-400 | Flood anonymous sessions to OOM |
| HTTP-046 | Eviction O(n) under flood | CWE-407 | Oldest-eviction scan is super-linear |
| HTTP-047 | Session not invalidated on logout | CWE-613 | Id stays valid after logout |
| HTTP-048 | Concurrent session limit absent | CWE-613 | Unlimited parallel sessions per user |
| HTTP-049 | Missing `__Host-` prefix | — | Sibling subdomain overwrites the cookie |
| HTTP-050 | Session data race | CWE-362 | Concurrent requests mutate one session table |

### API key / authorization

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-051 | Timing-unsafe key compare | CWE-208 | Byte-by-byte compare leaks the key |
| HTTP-052 | Key in URL/query | CWE-598 | Key logged/cached via query string |
| HTTP-053 | Empty/missing key accepted | CWE-287 | Blank key passes |
| HTTP-054 | BOLA / IDOR | CWE-639 | Swap object id to reach another user's data |
| HTTP-055 | BFLA function-level | CWE-285 | Normal user reaches an admin route |
| HTTP-056 | Mass assignment / BOPLA | CWE-915 | Extra fields (`role`, `isAdmin`) bound to model |
| HTTP-057 | Forced browsing | CWE-425 | Undisclosed endpoints lack authz |
| HTTP-058 | Path-normalization authz bypass | CWE-22 | Paths like `//admin`, `/./admin`, `%2e` evade prefix rules |
| HTTP-059 | Role from client claim | CWE-269 | Trust client-supplied role/scope |
| HTTP-060 | Middleware order bypass | CWE-696 | A route registered before the auth middleware |

### Input validation & injection

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-061 | Response header CRLF | CWE-113 | A `\r\n` in a set header injects headers |
| HTTP-062 | Response splitting | CWE-113 | CRLF smuggles a second response |
| HTTP-063 | Bare CR / bare LF | CWE-113 | Lone `\r` or `\n` in header value |
| HTTP-064 | Header name token violation | CWE-20 | Spaces/`:`/controls in a header name |
| HTTP-065 | Request smuggling CL.TE | CWE-444 | Front-end CL, back-end TE |
| HTTP-066 | Request smuggling TE.CL | CWE-444 | Front-end TE, back-end CL |
| HTTP-067 | Request smuggling TE.TE | CWE-444 | Obfuscated `Transfer-Encoding` |
| HTTP-068 | Duplicate Content-Length | CWE-444 | Two CL headers disagree |
| HTTP-069 | HTTP/2→1 downgrade desync | CWE-444 | The h2 fields smuggle an h1 request |
| HTTP-070 | Host header injection | CWE-644 | Spoofed `Host` poisons cache/reset links |
| HTTP-071 | Absolute-form request URI | CWE-444 | A `GET http://evil/ ...` request line confuses routing |
| HTTP-072 | Open redirect | CWE-601 | User-controlled `Location` |
| HTTP-073 | Open redirect via back-slash/`//` | CWE-601 | The `//evil.com`, `/\evil.com` schemes |
| HTTP-074 | Reflected XSS in error page | CWE-79 | Unescaped input echoed in an error body |
| HTTP-075 | XSS in directory listing | CWE-79 | Unescaped filename executes as HTML |
| HTTP-076 | Cookie value injection | CWE-113 | A `;`/CRLF in cookie value forges attributes |
| HTTP-077 | Cookie attribute injection | CWE-113 | Unsanitized `path`/`domain` adds attributes |
| HTTP-078 | `SameSite=None` without Secure | browser | Rejected-cookie / downgrade |
| HTTP-079 | HTTP parameter pollution | CWE-235 | The query `a=1&a=2` parsed inconsistently |
| HTTP-080 | Header/param duplicate collapse | CWE-436 | Last-wins loses earlier values |
| HTTP-081 | SQL injection (app) | CWE-89 | Unparameterized query from request |
| HTTP-082 | OS command injection (app) | CWE-78 | Request input reaches a shell |
| HTTP-083 | Template injection (app) | CWE-1336 | Input evaluated by a template engine |
| HTTP-084 | Log injection | CWE-117 | CRLF in a logged request field |
| HTTP-085 | Unicode/percent normalization mismatch | CWE-178 | Route vs static normalize differently |

### CSRF / CORS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-086 | Missing CSRF on unsafe method | CWE-352 | Mutation with only ambient cookies |
| HTTP-087 | Forgeable CSRF token | CWE-352 | Predictable/unsigned token |
| HTTP-088 | Naive double-submit / cookie tossing | CWE-352 | Sibling subdomain injects a matching cookie |
| HTTP-089 | CSRF token not session-bound | CWE-352 | A token from any session is accepted |
| HTTP-090 | Login CSRF | CWE-352 | Victim authenticated as the attacker |
| HTTP-091 | State change on GET/HEAD | CWE-650 | Safe method mutates state |
| HTTP-092 | CORS wildcard + credentials | CWE-942 | An `ACAO:*` with credentials |
| HTTP-093 | CORS reflected origin | CWE-942 | Any `Origin` echoed back |
| HTTP-094 | CORS null origin | CWE-942 | An `Origin: null` trusted |
| HTTP-095 | CORS substring/suffix match | CWE-942 | Origins like `trusted.com.evil`, `eviltrusted.com` |
| HTTP-096 | CORS missing `Vary: Origin` | CWE-942 | Cached response leaks an allowed origin |
| HTTP-097 | Preflight over-permissive | CWE-942 | Blanket 204 to any OPTIONS |

### Routing & path

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-098 | Control chars in path | CWE-74 | Raw control bytes desync matching |
| HTTP-099 | Route-constraint ReDoS | CWE-1333 | Backtracking `where()` regex + long segment |
| HTTP-100 | Param length DoS | CWE-400 | Giant path segment drives regex/copy work |
| HTTP-101 | HEAD vs GET mismatch | RFC 9110 | HEAD leaks a body or wrong headers |
| HTTP-102 | OPTIONS/405 `Allow` wrong | RFC 9110 | Missing/incorrect `Allow` |
| HTTP-103 | URL-build injection | CWE-116 | Unescaped params inject path segments |
| HTTP-104 | Route shadowing | CWE-696 | Overlapping precedence exposes a handler |
| HTTP-105 | Trailing-slash inconsistency | CWE-289 | The `/x` vs `/x/` authz/route mismatch |
| HTTP-106 | Empty-segment collapse | CWE-289 | A `//` collapsed differently than expected |
| HTTP-107 | Matrix/`;`-param confusion | CWE-20 | The `;param` segments alter matching |
| HTTP-108 | Case-sensitivity mismatch | CWE-178 | The `/Admin` vs `/admin` routing/authz |
| HTTP-109 | Wildcard/catch-all greediness | CWE-20 | A catch-all swallows protected paths |

### Static files

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-110 | Traversal `../` | CWE-22 | Dot-dot escapes the web root |
| HTTP-111 | Traversal percent-encoded | CWE-22 | The sequence `%2e%2e%2f` decodes to `../` |
| HTTP-112 | Traversal double-encoded | CWE-22 | The sequence `%252e%252e` |
| HTTP-113 | Traversal `....//` | CWE-22 | Single-strip leaves `../` |
| HTTP-114 | Traversal overlong UTF-8 | CWE-22 | The `%c0%ae` overlong dot |
| HTTP-115 | Traversal backslash (Windows) | CWE-22 | The `..\` separators |
| HTTP-116 | NUL-byte path truncation | CWE-626 | A path like `file.txt%00.png` |
| HTTP-117 | Absolute path | CWE-36 | Using `/etc/passwd` as request path |
| HTTP-118 | Symlink escape | CWE-59 | Symlink in root resolves outside |
| HTTP-119 | Dotfile/secret exposure | CWE-538 | Files like `.env`, `.git/`, backups served |
| HTTP-120 | MIME sniffing | CWE-430 | Missing `nosniff` runs uploaded HTML |
| HTTP-121 | Large-file memory blowup | CWE-400 | Whole-file read exhausts RAM |
| HTTP-122 | Disk I/O on event loop | CWE-400 | Large/slow read stalls all requests |
| HTTP-123 | Range over-allocation | CWE-789 | Huge `Range` over-allocates a buffer |
| HTTP-124 | Range overlap/negative | CWE-190 | A `bytes=-1` range, reversed ranges |
| HTTP-125 | Multi-range amplification | CWE-400 | Many ranges duplicate the body |
| HTTP-126 | Conditional bypass | RFC 9110 | Crafted `If-*` returns wrong 200/304 |
| HTTP-127 | Directory listing enabled | CWE-548 | Listing leaks file inventory |
| HTTP-128 | Case-insensitive FS bypass | CWE-178 | The `INDEX.HTML` vs blocked `index.html` |
| HTTP-129 | Trailing-dot/space FS bypass | CWE-289 | Both `secret. ` / `secret ` map to the same file |

### Body parsing & DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-130 | Unbounded body | CWE-400 | Huge request body exhausts memory |
| HTTP-131 | Form field flood | CWE-400 | Body of `&` separators |
| HTTP-132 | Multipart part flood | CWE-400 | Thousands of parts |
| HTTP-133 | Multipart huge headers | CWE-400 | Oversized part headers |
| HTTP-134 | Multipart boundary confusion | CWE-20 | A `name=` nested in `filename=` |
| HTTP-135 | Content-Type confusion | CWE-436 | Mismatched CT bypasses parser validation |
| HTTP-136 | charset/encoding confusion | CWE-176 | Declared vs actual charset mismatch |
| HTTP-137 | JSON body depth bomb | CWE-674 | Deeply nested JSON body |
| HTTP-138 | Slowloris (headers) | CWE-400 | Drip-fed headers hold connections |
| HTTP-139 | Slow body (R-U-Dead-Yet) | CWE-400 | Drip-fed body ties up workers |
| HTTP-140 | Missing/weak rate limiting | CWE-770 | Unthrottled requests exhaust backends |
| HTTP-141 | Spoofed client IP | CWE-348 | The `X-Forwarded-For` header evades per-IP limits |
| HTTP-142 | Rate-limit key collision | CWE-694 | IP equals a bookkeeping key |
| HTTP-143 | Large/abundant headers | CWE-400 | Oversized/numerous headers exhaust parser |
| HTTP-144 | Connection exhaustion | CWE-400 | Many idle keep-alives starve the pool |
| HTTP-145 | Hash-flood on param maps | CWE-407 | Colliding keys degrade lookups |
| HTTP-146 | Expect/100-continue abuse | CWE-400 | An `Expect: 100-continue` resource hold |
| HTTP-147 | Chunked-encoding bomb | CWE-400 | Huge chunk sizes / chunk-ext flood |

### Headers, config & TLS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-148 | Missing security headers | CWE-693 | No `nosniff`/frame/referrer |
| HTTP-149 | Missing/weak CSP | CWE-1021 | No CSP enables XSS/clickjacking |
| HTTP-150 | Missing HSTS | CWE-319 | SSL-strip downgrade |
| HTTP-151 | Missing COOP/COEP/CORP | CWE-1021 | Cross-origin isolation absent |
| HTTP-152 | Sensitive response cached | CWE-525 | Auth response lacks `no-store` |
| HTTP-153 | Verbose server banner | CWE-200 | Version disclosure |
| HTTP-154 | TLS legacy protocol | CWE-326 | SSLv3/TLS 1.0/1.1 negotiable |
| HTTP-155 | TLS weak ciphers | CWE-327 | RC4/3DES/EXPORT/NULL/MD5 |
| HTTP-156 | POODLE | CVE-2014-3566 | SSLv3 CBC padding oracle |
| HTTP-157 | BEAST | CVE-2011-3389 | TLS 1.0 CBC IV |
| HTTP-158 | CRIME/BREACH | CVE-2012-4929 | TLS/HTTP compression side-channel |
| HTTP-159 | Lucky13 | CVE-2013-0169 | CBC padding timing |
| HTTP-160 | Heartbleed / lib CVE | CVE-2014-0160 | OpenSSL memory disclosure |
| HTTP-161 | Insecure renegotiation | CVE-2009-3555 | Client reneg request injection |
| HTTP-162 | TLS downgrade | CWE-757 | MITM forces weaker suite |
| HTTP-163 | Missing cert chain/SNI validation | CWE-295 | Wrong vhost cert served |

### WebSocket

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-164 | Cross-site WS hijacking | CWE-1385 | No Origin check on upgrade |
| HTTP-165 | Unbounded message memory | CWE-400 | Fragmented stream assembles unbounded |
| HTTP-166 | Plaintext `ws://` secrets | CWE-319 | Tokens over unencrypted WS |
| HTTP-167 | Missing per-connection limits | CWE-770 | Frame/connection flooding |
| HTTP-168 | Frame injection | CWE-74 | Unvalidated frame data downstream |
| HTTP-169 | Compression bomb (permessage-deflate) | CWE-409 | Tiny frame expands hugely |
| HTTP-170 | Ping/pong flood | CWE-400 | Control-frame flooding |
| HTTP-171 | Slow WS read | CWE-400 | Drip-fed frames hold a worker |

### Memory / concurrency / fuzz (http internals)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-172 | Deferred-response race | CWE-362 | Worker flush races main-loop write/end |
| HTTP-173 | Context UAF after end | CWE-416 | Handler touches the context after the response ended |
| HTTP-174 | Double-end response | CWE-675 | Calling `end()`/`send()` twice |
| HTTP-175 | Chunk deque growth (slow client) | CWE-400 | Streaming chunks accumulate unbounded |
| HTTP-176 | Lua ref leak per request | CWE-401 | Thread/context/chain refs not unref'd |
| HTTP-177 | Handler exception safety | CWE-248 | C++ throw from a handler crosses the boundary |
| HTTP-178 | Status-code clamp | CWE-20 | Out-of-range status (`<100`/`>599`) |
| HTTP-179 | Request line / target fuzz | CWE-20 | Malformed method/target/version |
| HTTP-180 | Cookie header parse fuzz | CWE-20 | Malformed `Cookie:` crashes/leaks |
| HTTP-181 | Query string parse fuzz | CWE-20 | Malformed/huge query parsing |
| HTTP-182 | Multipart parser fuzz | CWE-20 | Malformed boundaries/headers |
| HTTP-183 | Shutdown during in-flight request | CWE-362 | Stop while a request streams |


---

## Additional cases (documented attacks & CVEs)

### Request smuggling / desync (named)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-184 | CL.0 desync | CWE-444 | Back-end ignores Content-Length (PortSwigger "Browser-Powered Desync") |
| HTTP-185 | 0.CL desync | CWE-444 | Front-end ignores CL, back-end honors it |
| HTTP-186 | H2.CL smuggling | CWE-444 | HTTP/2 with a smuggled Content-Length |
| HTTP-187 | H2.TE smuggling | CWE-444 | HTTP/2 with Transfer-Encoding |
| HTTP-188 | CRLF in h2 header value | CWE-113 | The h2 pseudo/headers carry CR/LF (HTTP/2 downgrade) |
| HTTP-189 | Client-side desync | CWE-444 | Victim browser poisons its own connection |
| HTTP-190 | Pause-based desync | CWE-444 | Timing the body to desync (PortSwigger 2022) |
| HTTP-191 | TE obfuscation tricks | CWE-444 | The `Transfer-Encoding: chunked\r\nX:` variants |
| HTTP-192 | Connection-header smuggling | CWE-444 | Hop-by-hop header abuse |
| HTTP-193 | Expect-based desync | CWE-444 | An `Expect: 100-continue` desync |

### Cache poisoning / deception

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-194 | Web cache poisoning (unkeyed header) | CWE-444 | The `X-Forwarded-Host` header reflected, cached |
| HTTP-195 | Web cache deception | CWE-525 | Requesting `/account/profile.css` caches private data |
| HTTP-196 | Cache key normalization | CWE-436 | Path/case normalization differs from cache |
| HTTP-197 | Fat GET cache poisoning | CWE-444 | Body on a GET cached inconsistently |
| HTTP-198 | Parameter cloaking | CWE-235 | The `;`/duplicate params split across cache layers |
| HTTP-199 | Vary mishandling | CWE-525 | Response varies but key doesn't |

### DoS (named, documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-200 | HTTP/2 Rapid Reset | CVE-2023-44487 | Stream open+RST flood |
| HTTP-201 | HTTP/2 CONTINUATION flood | CVE-2024-27316 | Endless CONTINUATION frames |
| HTTP-202 | HPACK bomb | CWE-409 | Compressed header expansion |
| HTTP-203 | HTTP/2 settings flood | CWE-400 | SETTINGS/PING/PRIORITY frame floods |
| HTTP-204 | Apache range DoS | CVE-2011-3192 | Overlapping `Range` "Apache Killer" |
| HTTP-205 | Slow POST (RUDY) | CWE-400 | Drip-fed body keeps connections |
| HTTP-206 | Slow read (window) | CWE-400 | Tiny receive window stalls the server |
| HTTP-207 | Hash-collision DoS | CVE-2011-3414 | Colliding POST params (oCERT-2011-003) |
| HTTP-208 | ReDoS in header parsing | CWE-1333 | Catastrophic regex over a header |
| HTTP-209 | Decompression bomb (gzip body) | CWE-409 | Compressed request body expands |
| HTTP-210 | Billion-laughs via JSON/XML body | CWE-776 | Nested body bomb |
| HTTP-211 | Cookie bomb | CWE-400 | Many/large cookies per request |
| HTTP-212 | Header count amplification | CWE-400 | Thousands of headers |

### Path / traversal (documented engines)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-213 | Apache 2.4.49 path traversal | CVE-2021-41773 | The `%2e%2e` traversal/RCE |
| HTTP-214 | Apache 2.4.50 bypass | CVE-2021-42013 | Double-encoded follow-up |
| HTTP-215 | IIS Unicode traversal | CVE-2000-0884 | The `%c0%af` overlong slash |
| HTTP-216 | nginx `alias` traversal | CWE-22 | Misconfigured `alias` off-by-slash |
| HTTP-217 | nginx `merge_slashes` off | CWE-22 | A `//` collapses past a guard |
| HTTP-218 | Tomcat AJP Ghostcat | CVE-2020-1938 | File read/inclusion via AJP |
| HTTP-219 | Spring path-pattern bypass | CWE-22 | The `..;/` matrix-segment traversal |
| HTTP-220 | Encoded-slash in path | CWE-22 | The `%2f` decoded after authz |
| HTTP-221 | Semicolon path param (Tomcat) | CWE-20 | The `;jsessionid` segment confusion |
| HTTP-222 | Trailing-dot host/path (Windows) | CWE-289 | The `index.jsp. ` source disclosure |

### Auth / session / JWT (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-223 | JWT `alg` array | CWE-347 | The `alg` as an array confuses validation |
| HTTP-224 | JWT JSON duplicate `alg` | CWE-347 | Duplicate header keys |
| HTTP-225 | JWT psychic signatures | CVE-2022-21449 | ECDSA `(0,0)` signature (lib-class) |
| HTTP-226 | JWT key-id SQL/LDAP | CWE-89 | The `kid` injection into a lookup |
| HTTP-227 | JWT billion-hashes (PBKDF) | CWE-400 | Costly `p2c` in JWE/JWK |
| HTTP-228 | Session puzzling | CWE-841 | A variable set in one flow trusted in another |
| HTTP-229 | Cookie `__Host-` bypass | CWE-565 | Crafted prefix defeats the check |
| HTTP-230 | Cookie sandwich/tossing | CWE-784 | Duplicate cookies pick the attacker's |
| HTTP-231 | Cross-site cookie shadowing | CWE-565 | Sibling subdomain shadows the cookie |
| HTTP-232 | OAuth/OIDC state CSRF | CWE-352 | Missing `state` in a callback |
| HTTP-233 | JWT `none` via mixed case nested | CWE-347 | A `nOnE` after a JSON unescape |

### Injection contexts (second-order, documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-234 | Second-order XSS | CWE-79 | Stored value rendered later |
| HTTP-235 | DOM/reflected via JSON response | CWE-79 | JSON reflected into HTML |
| HTTP-236 | Content-Type sniff XSS | CWE-430 | Text served, browser runs HTML |
| HTTP-237 | SVG/XML upload XSS | CWE-79 | Served SVG executes script |
| HTTP-238 | Reflected file download | CWE-binary | Attacker-named download runs |
| HTTP-239 | CSV/formula injection in export | CWE-1236 | A `=cmd` in an exported field |
| HTTP-240 | NoSQL injection (app) | CWE-943 | Operator injection in a query object |
| HTTP-241 | LDAP injection (app) | CWE-90 | Filter metacharacters |
| HTTP-242 | XPath injection (app) | CWE-643 | Query metacharacters |
| HTTP-243 | Email/SMTP header injection | CWE-93 | CRLF in an email field |
| HTTP-244 | Log4Shell-style lookup | CVE-2021-44228 | A `${jndi:...}` reaches a logger/evaluator |
| HTTP-245 | SSTI to RCE | CWE-1336 | Template expression executes |

### CORS / WebSocket / headers (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-246 | CORS `Origin` regex dot bypass | CWE-942 | A `.` in a regex matches any char |
| HTTP-247 | CORS scheme/port ignored | CWE-942 | HTTP vs HTTPS or port not checked |
| HTTP-248 | CORS post-domain wildcard | CWE-942 | An origin like `trusted.attacker.com` |
| HTTP-249 | CSWSH via cookie auth | CWE-1385 | Cross-site WS rides cookies |
| HTTP-250 | WebSocket smuggling | CWE-444 | Fake 101 upgrade tunnels requests |
| HTTP-251 | WS compression bomb | CVE-class | Permessage-deflate expansion |
| HTTP-252 | Missing `X-Frame-Options`/`frame-ancestors` | CWE-1021 | Clickjacking |
| HTTP-253 | Dangling-markup injection | CWE-79 | Unterminated attribute exfiltrates |
| HTTP-254 | Reverse tabnabbing | CWE-1022 | A `target=_blank` without `noopener` |
| HTTP-255 | Mixed-content downgrade | CWE-319 | An HTTP subresource on an HTTPS page |

### Business logic / misc (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-256 | Race-condition limit bypass | CWE-362 | Parallel requests double-spend (TOCTOU) |
| HTTP-257 | Mass-assignment privilege | CWE-915 | Extra `is_admin` field bound |
| HTTP-258 | IDOR on numeric id | CWE-639 | Increment id to reach others |
| HTTP-259 | GUID/UUID predictability | CWE-340 | Guessable v1 UUIDs |
| HTTP-260 | Password-reset host poisoning | CWE-640 | The `Host` header poisons the reset link |
| HTTP-261 | Open redirect → token theft | CWE-601 | OAuth token leaked via redirect |
| HTTP-262 | Rate-limit reset via header | CWE-348 | Spoofed `X-Forwarded-For` resets buckets |
| HTTP-263 | Email enumeration | CWE-204 | Response/timing differs for valid users |
| HTTP-264 | Username enumeration via timing | CWE-208 | Login timing leaks validity |
| HTTP-265 | Verbose error stack leak | CWE-209 | A 500 exposes internals |
| HTTP-266 | Debug endpoint exposed | CWE-489 | A left-on debug/admin route |
| HTTP-267 | Directory indexing leak | CWE-548 | Auto-index reveals files |
| HTTP-268 | Backup/temp file exposure | CWE-530 | Files like `.bak`/`~`/`.swp` served |
| HTTP-269 | Source disclosure via encoding | CWE-540 | A `%00`/null served raw source |
| HTTP-270 | Method override abuse | CWE-650 | The `X-HTTP-Method-Override` header bypasses authz |

### Protocol & state (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-271 | Connection reuse after auth | CWE-441 | A pooled conn keeps another user's identity |
| HTTP-272 | TLS session resumption confusion | CWE-441 | Resumed session crosses identities |
| HTTP-273 | Early-data (0-RTT) replay | CWE-294 | TLS 1.3 0-RTT request replay |
| HTTP-274 | ALPN/protocol confusion | CWE-436 | Negotiated protocol mismatch |
| HTTP-275 | Trailer header smuggling | CWE-444 | Chunked trailers smuggle headers |
| HTTP-276 | Absolute-URI vs Host mismatch | CWE-444 | Request-target vs Host disagree |
| HTTP-277 | Pipelining response queue poisoning | CWE-444 | Queued responses misaligned |
| HTTP-278 | Keep-alive timeout race | CWE-362 | Response written to a reused socket |
| HTTP-279 | 100-continue body handling | CWE-444 | Body sent before/after continue |
| HTTP-280 | Invalid chunk-size hex | CWE-444 | Non-hex/oversized chunk length |
| HTTP-281 | Negative/huge Content-Length | CWE-190 | CL parsing overflow |
| HTTP-282 | Duplicate Host headers | CWE-444 | Two `Host` values disagree |
| HTTP-283 | Whitespace before colon | CWE-444 | The `Header : value` parsing variance |

---

## Round 3 — deeper / documented

### Smuggling & connection-level (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-284 | Visible normalization desync | CWE-444 | Edge normalizes a header the origin keeps |
| HTTP-285 | Header-name case desync | CWE-444 | The `Transfer-Encoding` vs `transfer-encoding` |
| HTTP-286 | Tab-as-space TE obfuscation | CWE-444 | A `Transfer-Encoding:\tchunked` header |
| HTTP-287 | Vertical-tab/FF in headers | CWE-444 | Exotic whitespace splits parsers |
| HTTP-288 | Double Transfer-Encoding | CWE-444 | Two TE headers disagree |
| HTTP-289 | Chunk extension smuggling | CWE-444 | The `1;ext=...` chunk metadata |
| HTTP-290 | Last-chunk trailer injection | CWE-444 | Headers after `0\r\n` |
| HTTP-291 | Connection: keep-alive desync | CWE-444 | Hop-by-hop directive abuse |
| HTTP-292 | Pipelined-request boundary | CWE-444 | Second request consumed as body |
| HTTP-293 | HTTP/1.0 with Keep-Alive | CWE-444 | Version/keep-alive mismatch |

### Cache & proxy (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-294 | Unkeyed cookie cache poison | CWE-525 | Cookie reflected into a cached page |
| HTTP-295 | Cache-key path-confusion | CWE-436 | The `;`/`%2f` differs from the cache key |
| HTTP-296 | Cache poisoning via 404 | CWE-525 | Cached error reflects input |
| HTTP-297 | Range-cache poisoning | CWE-525 | Partial response cached as full |
| HTTP-298 | Vary header omission | CWE-525 | Content-negotiated response mis-cached |
| HTTP-299 | CDN-origin header trust | CWE-444 | Origin trusts a forwarded header |
| HTTP-300 | Stale-while-revalidate abuse | CWE-525 | Serving stale sensitive data |

### Auth / session / token (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-301 | JWT `kid` header path traversal | CWE-22 | The `kid` reads `/dev/null` to force an empty key |
| HTTP-302 | JWT zip-bomb in JWE | CWE-409 | Compressed JWE payload |
| HTTP-303 | JWT `b64:false` (RFC 7797) | CWE-347 | Unencoded-payload option abuse |
| HTTP-304 | Bearer vs cookie precedence | CWE-287 | Two credentials, wrong one trusted |
| HTTP-305 | Session id in URL | CWE-598 | Id logged/leaked via Referer |
| HTTP-306 | Referer leaks token | CWE-200 | Token in URL leaks cross-origin |
| HTTP-307 | Remember-me token theft | CWE-539 | Long-lived token weakly protected |
| HTTP-308 | Step-up auth bypass | CWE-287 | Sensitive action skips re-auth |
| HTTP-309 | Concurrent-login fixation | CWE-384 | Parallel sessions share an id |
| HTTP-310 | CSRF token not rotated | CWE-352 | Token reuse across privilege change |
| HTTP-311 | Double-submit subdomain inject | CWE-352 | Sibling sets the CSRF cookie |
| HTTP-312 | `SameSite=Lax` GET mutation | CWE-352 | Top-level GET still sends the cookie |

### Injection contexts (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-313 | GraphQL injection | CWE-943 | Query/operation injection |
| HTTP-314 | GraphQL batch/alias DoS | CWE-770 | Aliased query amplification |
| HTTP-315 | GraphQL introspection leak | CWE-200 | Schema exposed |
| HTTP-316 | XPath/XQuery injection | CWE-643 | Query metacharacters |
| HTTP-317 | SSI injection | CWE-97 | Server-side includes |
| HTTP-318 | ESI injection | CWE-74 | Edge-side includes |
| HTTP-319 | CSV/TSV export formula | CWE-1236 | A `=`/`@`/`+` cell prefix |
| HTTP-320 | Header value injection into log | CWE-117 | Reflected to a log sink |
| HTTP-321 | Reflected content-type XSS | CWE-430 | Echoed type drives execution |
| HTTP-322 | Polyglot upload (GIFAR) | CWE-434 | File valid as image and archive |
| HTTP-323 | SVG/HTML upload stored XSS | CWE-79 | Served upload runs script |
| HTTP-324 | ZIP/path in multipart filename | CWE-22 | A `../` in an upload filename |

### File / static (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-325 | If-Range cache bypass | RFC 9110 | Mismatched validator returns full body |
| HTTP-326 | Multi-range memory amplification | CWE-400 | Many small ranges |
| HTTP-327 | Range integer overflow | CWE-190 | Start+length overflow |
| HTTP-328 | Conditional-request validator confusion | RFC 9110 | Weak vs strong ETag |
| HTTP-329 | Symlinked `publicDir` root | CWE-59 | The root itself is a symlink |
| HTTP-330 | Case-insensitive dotfile bypass | CWE-178 | The `.ENV` vs blocked `.env` |
| HTTP-331 | Encoded dotfile request | CWE-22 | The request `%2e%67it/config` |
| HTTP-332 | Index file shadowing | CWE-424 | Uploaded `index.html` overrides |
| HTTP-333 | Content-Disposition RFC 6266 edge | CWE-116 | Encoding abuse of `filename*` |
| HTTP-334 | Large directory listing DoS | CWE-400 | Huge folder render |

### DoS & resource (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-335 | ReDoS in route constraint | CWE-1333 | Catastrophic `where()` regex |
| HTTP-336 | ReDoS in user pattern | CWE-1333 | App-supplied Lua pattern |
| HTTP-337 | Multipart part-count flood | CWE-400 | Thousands of parts |
| HTTP-338 | Nested multipart | CWE-400 | Multipart inside multipart |
| HTTP-339 | JSON key-count flood | CWE-407 | Many object keys in a body |
| HTTP-340 | Query-parameter flood | CWE-400 | Thousands of query params |
| HTTP-341 | Cookie-count flood | CWE-400 | Many cookies per request |
| HTTP-342 | Header-line length flood | CWE-400 | One giant header line |
| HTTP-343 | Keep-alive socket hoarding | CWE-400 | Idle connections held |
| HTTP-344 | Per-IP rate-limit spoof | CWE-348 | Forged forwarded address |
| HTTP-345 | Rate-limit memory growth | CWE-400 | Unbounded per-IP buckets |
| HTTP-346 | Async-handler leak under load | CWE-401 | Coroutine refs accumulate |

### Headers / TLS / transport (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-347 | CSP bypass via JSONP/nonce reuse | CWE-1021 | Weak/static CSP nonce |
| HTTP-348 | CSP `unsafe-inline`/`eval` | CWE-1021 | Permissive policy |
| HTTP-349 | HSTS not preloaded | CWE-319 | First-visit downgrade |
| HTTP-350 | Permissions-Policy missing | CWE-693 | Powerful features ungated |
| HTTP-351 | Referrer-Policy leak | CWE-200 | Full URL leaked cross-origin |
| HTTP-352 | X-Frame vs `frame-ancestors` gap | CWE-1021 | Clickjacking via legacy gap |
| HTTP-353 | TLS session-ticket key rotation | CWE-323 | Static ticket key |
| HTTP-354 | OCSP stapling absent | CWE-299 | Revocation not advertised |
| HTTP-355 | ALPN downgrade | CWE-757 | Protocol negotiation tampering |
| HTTP-356 | 0-RTT early-data on mutating route | CWE-294 | Replayable early data |
| HTTP-357 | Mixed HTTP/HTTPS listener confusion | CWE-319 | Plaintext sibling listener |

### WebSocket & streaming (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-358 | WS subprotocol confusion | CWE-436 | Negotiated subprotocol mismatch |
| HTTP-359 | WS extension negotiation abuse | CWE-409 | Permessage-deflate bomb |
| HTTP-360 | WS masking not enforced | CWE-20 | Unmasked client frame accepted |
| HTTP-361 | WS fragmented control frame | CWE-20 | Control frame fragmentation |
| HTTP-362 | WS reserved-bit handling | CWE-20 | RSV bits set without extension |
| HTTP-363 | WS close-code abuse | CWE-20 | Invalid close codes |
| HTTP-364 | WS UTF-8 validation (text frame) | CWE-176 | Invalid UTF-8 in a text frame |
| HTTP-365 | WS ping/pong flood | CWE-400 | Control-frame flooding |
| HTTP-366 | Chunked-stream backpressure | CWE-400 | Slow client stalls a streamed response |
| HTTP-367 | SSE connection hoarding | CWE-400 | Many event-stream connections |

### Business logic & API (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-368 | TOCTOU on balance/quota | CWE-367 | Parallel spend race |
| HTTP-369 | Idempotency-key bypass | CWE-696 | Duplicate side effects |
| HTTP-370 | Negative-quantity order | CWE-840 | Negative value flips logic |
| HTTP-371 | Price/parameter tampering | CWE-472 | Client-controlled price field |
| HTTP-372 | Coupon/replay abuse | CWE-837 | Reuse a single-use token |
| HTTP-373 | Workflow step skipping | CWE-840 | Jump past a required step |
| HTTP-374 | Excessive data exposure | CWE-213 (API3) | Endpoint returns extra fields |
| HTTP-375 | Unrestricted business flow | API6 | Automation abuses a sensitive flow |
| HTTP-376 | Improper inventory (shadow API) | API9 | Undocumented/old version reachable |
| HTTP-377 | Unsafe API consumption | API10 | Trusting a third-party response |
| HTTP-378 | Pagination resource abuse | CWE-770 | Huge page-size requests |
| HTTP-379 | Sort/filter injection | CWE-89 | Order-by from input |

### Misc / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-380 | Request-line fuzz | CWE-20 | Malformed method/target/version |
| HTTP-381 | Header-block fuzz | CWE-20 | Malformed/huge header sets |
| HTTP-382 | Body-grammar fuzz (json/form/multipart) | CWE-20 | Structure-aware body fuzzing |
| HTTP-383 | Differential vs reference server | CWE-697 | Parsing diverges from a known server |
