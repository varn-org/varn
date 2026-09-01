# 🌐 http — server & app framework

### JWT & token auth

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-001 | `alg:none` (lowercase) | CWE-347 | `{"alg":"none"}` with empty signature accepted |
| HTTP-002 | `alg:None`/`NONE` case variants | CWE-347 | case-insensitive `none` bypasses a naive check |
| HTTP-003 | Algorithm confusion RS256→HS256 | CWE-347 | sign with the RSA public key as the HMAC secret |
| HTTP-004 | Algorithm confusion ES256→HS256 | CWE-347 | EC public key used as HMAC key |
| HTTP-005 | Algorithm downgrade HS512→HS256 | CWE-347 | swap to a weaker accepted alg |
| HTTP-006 | Empty signature, valid alg | CWE-347 | strip the signature but keep `alg:HS256` |
| HTTP-007 | Signature truncation | CWE-347 | drop bytes from the MAC; check rejects |
| HTTP-008 | Signature byte-flip | CWE-345 | flip one MAC bit; must reject |
| HTTP-009 | Payload tamper (role/sub) | CWE-345 | edit claims without re-signing |
| HTTP-010 | Weak secret brute force | CWE-326 | short/dictionary HMAC secret cracked offline |
| HTTP-011 | Empty secret accepted | CWE-347 | verifying with `""` must never succeed |
| HTTP-012 | `jwk` header injection | CWE-347 | attacker public key embedded in header |
| HTTP-013 | `jku`/`x5u` SSRF/key swap | CWE-918/347 | key-set URL points at attacker host |
| HTTP-014 | `kid` path traversal | CWE-22 | `kid` as a file path loads attacker key |
| HTTP-015 | `kid` SQL injection | CWE-89 | `kid` used in a key-lookup query |
| HTTP-016 | `kid` command injection | CWE-78 | `kid` reaches a shell during key load |
| HTTP-017 | `x5c` cert injection | CWE-347 | self-signed cert trusted as anchor |
| HTTP-018 | `cty` nested-JWT abuse | CWE-20 | `cty` triggers nested parse/deserialization |
| HTTP-019 | `typ` confusion | CWE-20 | unexpected `typ` accepted where it shouldn't |
| HTTP-020 | `crit` header ignored | RFC 8725 | unknown critical extension not rejected |
| HTTP-021 | Expired token (`exp`) | CWE-613 | past `exp` accepted |
| HTTP-022 | Missing `exp` | CWE-613 | token never expires |
| HTTP-023 | `nbf` in the future | CWE-345 | not-yet-valid token accepted |
| HTTP-024 | Clock-skew over-tolerance | CWE-613 | excessive leeway accepts long-expired tokens |
| HTTP-025 | `exp`/`nbf` non-numeric | CWE-20 | string/object `exp` mis-handled |
| HTTP-026 | Audience not validated | CWE-287 | token for service A accepted by B |
| HTTP-027 | Issuer not validated | CWE-287 | untrusted `iss` accepted |
| HTTP-028 | Subject confusion | CWE-287 | missing/duplicate `sub` handling |
| HTTP-029 | Non-object payload | CWE-20 | JSON-array payload indexed as claims |
| HTTP-030 | Token replay / no revocation | CWE-294 | reuse after logout/compromise |
| HTTP-031 | Cross-JWT confusion | CWE-287 | token from another app/realm accepted |
| HTTP-032 | base64url malleability | CWE-347 | non-canonical base64 segments still verify |
| HTTP-033 | Unicode/whitespace in token | CWE-20 | padded/whitespace token bypasses parsing |
| HTTP-034 | Sign mutates caller input | CWE-664 | signing injects claims into the caller's object |
| HTTP-035 | Bearer parsing leniency | CWE-20 | `bearer`/extra spaces/empty token accepted |

### Sessions

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-036 | Predictable session ID | CWE-330 | guessable/sequential ids |
| HTTP-037 | Insufficient ID entropy | CWE-331 | < 128-bit id is brute-forceable |
| HTTP-038 | Session fixation | CWE-384 | id not rotated on privilege change |
| HTTP-039 | Fixation via cookie injection | CWE-384 | attacker-set id survives login |
| HTTP-040 | Missing `HttpOnly` | CWE-1004 | XSS reads the session cookie |
| HTTP-041 | Missing `Secure` (under TLS) | CWE-614 | cookie sent over plaintext |
| HTTP-042 | Missing/weak `SameSite` | CWE-1275 | CSRF via cross-site cookie send |
| HTTP-043 | No idle timeout | CWE-613 | abandoned session valid forever |
| HTTP-044 | No absolute timeout | CWE-613 | active session never expires |
| HTTP-045 | Session store exhaustion | CWE-400 | flood anonymous sessions to OOM |
| HTTP-046 | Eviction O(n) under flood | CWE-407 | oldest-eviction scan is super-linear |
| HTTP-047 | Session not invalidated on logout | CWE-613 | id stays valid after logout |
| HTTP-048 | Concurrent session limit absent | CWE-613 | unlimited parallel sessions per user |
| HTTP-049 | Missing `__Host-` prefix | — | sibling subdomain overwrites the cookie |
| HTTP-050 | Session data race | CWE-362 | concurrent requests mutate one session table |

### API key / authorization

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-051 | Timing-unsafe key compare | CWE-208 | byte-by-byte compare leaks the key |
| HTTP-052 | Key in URL/query | CWE-598 | key logged/cached via query string |
| HTTP-053 | Empty/missing key accepted | CWE-287 | blank key passes |
| HTTP-054 | BOLA / IDOR | CWE-639 | swap object id to reach another user's data |
| HTTP-055 | BFLA function-level | CWE-285 | normal user reaches an admin route |
| HTTP-056 | Mass assignment / BOPLA | CWE-915 | extra fields (`role`, `isAdmin`) bound to model |
| HTTP-057 | Forced browsing | CWE-425 | undisclosed endpoints lack authz |
| HTTP-058 | Path-normalization authz bypass | CWE-22 | `//admin`, `/./admin`, `%2e` evade prefix rules |
| HTTP-059 | Role from client claim | CWE-269 | trust client-supplied role/scope |
| HTTP-060 | Middleware order bypass | CWE-696 | a route registered before the auth middleware |

### Input validation & injection

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-061 | Response header CRLF | CWE-113 | `\r\n` in a set header injects headers |
| HTTP-062 | Response splitting | CWE-113 | CRLF smuggles a second response |
| HTTP-063 | Bare CR / bare LF | CWE-113 | lone `\r` or `\n` in header value |
| HTTP-064 | Header name token violation | CWE-20 | spaces/`:`/controls in a header name |
| HTTP-065 | Request smuggling CL.TE | CWE-444 | front-end CL, back-end TE |
| HTTP-066 | Request smuggling TE.CL | CWE-444 | front-end TE, back-end CL |
| HTTP-067 | Request smuggling TE.TE | CWE-444 | obfuscated `Transfer-Encoding` |
| HTTP-068 | Duplicate Content-Length | CWE-444 | two CL headers disagree |
| HTTP-069 | HTTP/2→1 downgrade desync | CWE-444 | h2 fields smuggle an h1 request |
| HTTP-070 | Host header injection | CWE-644 | spoofed `Host` poisons cache/reset links |
| HTTP-071 | Absolute-form request URI | CWE-444 | `GET http://evil/ ...` confuses routing |
| HTTP-072 | Open redirect | CWE-601 | user-controlled `Location` |
| HTTP-073 | Open redirect via back-slash/`//` | CWE-601 | `//evil.com`, `/\evil.com` schemes |
| HTTP-074 | Reflected XSS in error page | CWE-79 | unescaped input echoed in an error body |
| HTTP-075 | XSS in directory listing | CWE-79 | unescaped filename executes as HTML |
| HTTP-076 | Cookie value injection | CWE-113 | `;`/CRLF in cookie value forges attributes |
| HTTP-077 | Cookie attribute injection | CWE-113 | unsanitized `path`/`domain` adds attributes |
| HTTP-078 | `SameSite=None` without Secure | browser | rejected-cookie / downgrade |
| HTTP-079 | HTTP parameter pollution | CWE-235 | `a=1&a=2` parsed inconsistently |
| HTTP-080 | Header/param duplicate collapse | CWE-436 | last-wins loses earlier values |
| HTTP-081 | SQL injection (app) | CWE-89 | unparameterized query from request |
| HTTP-082 | OS command injection (app) | CWE-78 | request input reaches a shell |
| HTTP-083 | Template injection (app) | CWE-1336 | input evaluated by a template engine |
| HTTP-084 | Log injection | CWE-117 | CRLF in a logged request field |
| HTTP-085 | Unicode/percent normalization mismatch | CWE-178 | route vs static normalize differently |

### CSRF / CORS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-086 | Missing CSRF on unsafe method | CWE-352 | mutation with only ambient cookies |
| HTTP-087 | Forgeable CSRF token | CWE-352 | predictable/unsigned token |
| HTTP-088 | Naive double-submit / cookie tossing | CWE-352 | sibling subdomain injects a matching cookie |
| HTTP-089 | CSRF token not session-bound | CWE-352 | a token from any session is accepted |
| HTTP-090 | Login CSRF | CWE-352 | victim authenticated as the attacker |
| HTTP-091 | State change on GET/HEAD | CWE-650 | safe method mutates state |
| HTTP-092 | CORS wildcard + credentials | CWE-942 | `ACAO:*` with credentials |
| HTTP-093 | CORS reflected origin | CWE-942 | any `Origin` echoed back |
| HTTP-094 | CORS null origin | CWE-942 | `Origin: null` trusted |
| HTTP-095 | CORS substring/suffix match | CWE-942 | `trusted.com.evil`, `eviltrusted.com` |
| HTTP-096 | CORS missing `Vary: Origin` | CWE-942 | cached response leaks an allowed origin |
| HTTP-097 | Preflight over-permissive | CWE-942 | blanket 204 to any OPTIONS |

### Routing & path

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-098 | Control chars in path | CWE-74 | raw control bytes desync matching |
| HTTP-099 | Route-constraint ReDoS | CWE-1333 | backtracking `where()` regex + long segment |
| HTTP-100 | Param length DoS | CWE-400 | giant path segment drives regex/copy work |
| HTTP-101 | HEAD vs GET mismatch | RFC 9110 | HEAD leaks a body or wrong headers |
| HTTP-102 | OPTIONS/405 `Allow` wrong | RFC 9110 | missing/incorrect `Allow` |
| HTTP-103 | URL-build injection | CWE-116 | unescaped params inject path segments |
| HTTP-104 | Route shadowing | CWE-696 | overlapping precedence exposes a handler |
| HTTP-105 | Trailing-slash inconsistency | CWE-289 | `/x` vs `/x/` authz/route mismatch |
| HTTP-106 | Empty-segment collapse | CWE-289 | `//` collapsed differently than expected |
| HTTP-107 | Matrix/`;`-param confusion | CWE-20 | `;param` segments alter matching |
| HTTP-108 | Case-sensitivity mismatch | CWE-178 | `/Admin` vs `/admin` routing/authz |
| HTTP-109 | Wildcard/catch-all greediness | CWE-20 | a catch-all swallows protected paths |

### Static files

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-110 | Traversal `../` | CWE-22 | dot-dot escapes the web root |
| HTTP-111 | Traversal percent-encoded | CWE-22 | `%2e%2e%2f` decodes to `../` |
| HTTP-112 | Traversal double-encoded | CWE-22 | `%252e%252e` |
| HTTP-113 | Traversal `....//` | CWE-22 | single-strip leaves `../` |
| HTTP-114 | Traversal overlong UTF-8 | CWE-22 | `%c0%ae` overlong dot |
| HTTP-115 | Traversal backslash (Windows) | CWE-22 | `..\` separators |
| HTTP-116 | NUL-byte path truncation | CWE-626 | `file.txt%00.png` |
| HTTP-117 | Absolute path | CWE-36 | `/etc/passwd` as request path |
| HTTP-118 | Symlink escape | CWE-59 | symlink in root resolves outside |
| HTTP-119 | Dotfile/secret exposure | CWE-538 | `.env`, `.git/`, backups served |
| HTTP-120 | MIME sniffing | CWE-430 | missing `nosniff` runs uploaded HTML |
| HTTP-121 | Large-file memory blowup | CWE-400 | whole-file read exhausts RAM |
| HTTP-122 | Disk I/O on event loop | CWE-400 | large/slow read stalls all requests |
| HTTP-123 | Range over-allocation | CWE-789 | huge `Range` over-allocates a buffer |
| HTTP-124 | Range overlap/negative | CWE-190 | `bytes=-1`, reversed ranges |
| HTTP-125 | Multi-range amplification | CWE-400 | many ranges duplicate the body |
| HTTP-126 | Conditional bypass | RFC 9110 | crafted `If-*` returns wrong 200/304 |
| HTTP-127 | Directory listing enabled | CWE-548 | listing leaks file inventory |
| HTTP-128 | Case-insensitive FS bypass | CWE-178 | `INDEX.HTML` vs blocked `index.html` |
| HTTP-129 | Trailing-dot/space FS bypass | CWE-289 | `secret.` / `secret ` map to the same file |

### Body parsing & DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-130 | Unbounded body | CWE-400 | huge request body exhausts memory |
| HTTP-131 | Form field flood | CWE-400 | body of `&` separators |
| HTTP-132 | Multipart part flood | CWE-400 | thousands of parts |
| HTTP-133 | Multipart huge headers | CWE-400 | oversized part headers |
| HTTP-134 | Multipart boundary confusion | CWE-20 | `name=` nested in `filename=` |
| HTTP-135 | Content-Type confusion | CWE-436 | mismatched CT bypasses parser validation |
| HTTP-136 | charset/encoding confusion | CWE-176 | declared vs actual charset mismatch |
| HTTP-137 | JSON body depth bomb | CWE-674 | deeply nested JSON body |
| HTTP-138 | Slowloris (headers) | CWE-400 | drip-fed headers hold connections |
| HTTP-139 | Slow body (R-U-Dead-Yet) | CWE-400 | drip-fed body ties up workers |
| HTTP-140 | Missing/weak rate limiting | CWE-770 | unthrottled requests exhaust backends |
| HTTP-141 | Spoofed client IP | CWE-348 | `X-Forwarded-For` evades per-IP limits |
| HTTP-142 | Rate-limit key collision | CWE-694 | IP equals a bookkeeping key |
| HTTP-143 | Large/abundant headers | CWE-400 | oversized/numerous headers exhaust parser |
| HTTP-144 | Connection exhaustion | CWE-400 | many idle keep-alives starve the pool |
| HTTP-145 | Hash-flood on param maps | CWE-407 | colliding keys degrade lookups |
| HTTP-146 | Expect/100-continue abuse | CWE-400 | `Expect: 100-continue` resource hold |
| HTTP-147 | Chunked-encoding bomb | CWE-400 | huge chunk sizes / chunk-ext flood |

### Headers, config & TLS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-148 | Missing security headers | CWE-693 | no nosniff/frame/referrer |
| HTTP-149 | Missing/weak CSP | CWE-1021 | no CSP enables XSS/clickjacking |
| HTTP-150 | Missing HSTS | CWE-319 | SSL-strip downgrade |
| HTTP-151 | Missing COOP/COEP/CORP | CWE-1021 | cross-origin isolation absent |
| HTTP-152 | Sensitive response cached | CWE-525 | auth response lacks `no-store` |
| HTTP-153 | Verbose server banner | CWE-200 | version disclosure |
| HTTP-154 | TLS legacy protocol | CWE-326 | SSLv3/TLS 1.0/1.1 negotiable |
| HTTP-155 | TLS weak ciphers | CWE-327 | RC4/3DES/EXPORT/NULL/MD5 |
| HTTP-156 | POODLE | CVE-2014-3566 | SSLv3 CBC padding oracle |
| HTTP-157 | BEAST | CVE-2011-3389 | TLS 1.0 CBC IV |
| HTTP-158 | CRIME/BREACH | CVE-2012-4929 | TLS/HTTP compression side-channel |
| HTTP-159 | Lucky13 | CVE-2013-0169 | CBC padding timing |
| HTTP-160 | Heartbleed / lib CVE | CVE-2014-0160 | OpenSSL memory disclosure |
| HTTP-161 | Insecure renegotiation | CVE-2009-3555 | client reneg request injection |
| HTTP-162 | TLS downgrade | CWE-757 | MITM forces weaker suite |
| HTTP-163 | Missing cert chain/SNI validation | CWE-295 | wrong vhost cert served |

### WebSocket

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-164 | Cross-site WS hijacking | CWE-1385 | no Origin check on upgrade |
| HTTP-165 | Unbounded message memory | CWE-400 | fragmented stream assembles unbounded |
| HTTP-166 | Plaintext ws:// secrets | CWE-319 | tokens over unencrypted WS |
| HTTP-167 | Missing per-connection limits | CWE-770 | frame/connection flooding |
| HTTP-168 | Frame injection | CWE-74 | unvalidated frame data downstream |
| HTTP-169 | Compression bomb (permessage-deflate) | CWE-409 | tiny frame expands hugely |
| HTTP-170 | Ping/pong flood | CWE-400 | control-frame flooding |
| HTTP-171 | Slow WS read | CWE-400 | drip-fed frames hold a worker |

### Memory / concurrency / fuzz (http internals)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-172 | Deferred-response race | CWE-362 | worker flush races main-loop write/end |
| HTTP-173 | Context UAF after end | CWE-416 | handler touches the context after the response ended |
| HTTP-174 | Double-end response | CWE-675 | `end()`/`send()` called twice |
| HTTP-175 | Chunk deque growth (slow client) | CWE-400 | streaming chunks accumulate unbounded |
| HTTP-176 | Lua ref leak per request | CWE-401 | thread/context/chain refs not unref'd |
| HTTP-177 | Handler exception safety | CWE-248 | C++ throw from a handler crosses the boundary |
| HTTP-178 | Status-code clamp | CWE-20 | out-of-range status (`<100`/`>599`) |
| HTTP-179 | Request line / target fuzz | CWE-20 | malformed method/target/version |
| HTTP-180 | Cookie header parse fuzz | CWE-20 | malformed `Cookie:` crashes/leaks |
| HTTP-181 | Query string parse fuzz | CWE-20 | malformed/huge query parsing |
| HTTP-182 | Multipart parser fuzz | CWE-20 | malformed boundaries/headers |
| HTTP-183 | Shutdown during in-flight request | CWE-362 | stop while a request streams |


---

## Additional cases (documented attacks & CVEs)

### Request smuggling / desync (named)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-184 | CL.0 desync | CWE-444 | back-end ignores Content-Length (PortSwigger "Browser-Powered Desync") |
| HTTP-185 | 0.CL desync | CWE-444 | front-end ignores CL, back-end honors it |
| HTTP-186 | H2.CL smuggling | CWE-444 | HTTP/2 with a smuggled Content-Length |
| HTTP-187 | H2.TE smuggling | CWE-444 | HTTP/2 with Transfer-Encoding |
| HTTP-188 | CRLF in h2 header value | CWE-113 | h2 pseudo/headers carry CR/LF (HTTP/2 downgrade) |
| HTTP-189 | Client-side desync | CWE-444 | victim browser poisons its own connection |
| HTTP-190 | Pause-based desync | CWE-444 | timing the body to desync (PortSwigger 2022) |
| HTTP-191 | TE obfuscation tricks | CWE-444 | `Transfer-Encoding: chunked\r\nX:` variants |
| HTTP-192 | Connection-header smuggling | CWE-444 | hop-by-hop header abuse |
| HTTP-193 | Expect-based desync | CWE-444 | `Expect: 100-continue` desync |

### Cache poisoning / deception

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-194 | Web cache poisoning (unkeyed header) | CWE-444 | `X-Forwarded-Host` reflected, cached |
| HTTP-195 | Web cache deception | CWE-525 | `/account/profile.css` caches private data |
| HTTP-196 | Cache key normalization | CWE-436 | path/case normalization differs from cache |
| HTTP-197 | Fat GET cache poisoning | CWE-444 | body on a GET cached inconsistently |
| HTTP-198 | Parameter cloaking | CWE-235 | `;`/duplicate params split across cache layers |
| HTTP-199 | Vary mishandling | CWE-525 | response varies but key doesn't |

### DoS (named, documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-200 | HTTP/2 Rapid Reset | CVE-2023-44487 | stream open+RST flood |
| HTTP-201 | HTTP/2 CONTINUATION flood | CVE-2024-27316 | endless CONTINUATION frames |
| HTTP-202 | HPACK bomb | CWE-409 | compressed header expansion |
| HTTP-203 | HTTP/2 settings flood | CWE-400 | SETTINGS/PING/PRIORITY frame floods |
| HTTP-204 | Apache range DoS | CVE-2011-3192 | overlapping `Range` "Apache Killer" |
| HTTP-205 | Slow POST (RUDY) | CWE-400 | drip-fed body keeps connections |
| HTTP-206 | Slow read (window) | CWE-400 | tiny receive window stalls the server |
| HTTP-207 | Hash-collision DoS | CVE-2011-3414 | colliding POST params (oCERT-2011-003) |
| HTTP-208 | ReDoS in header parsing | CWE-1333 | catastrophic regex over a header |
| HTTP-209 | Decompression bomb (gzip body) | CWE-409 | compressed request body expands |
| HTTP-210 | Billion-laughs via JSON/XML body | CWE-776 | nested body bomb |
| HTTP-211 | Cookie bomb | CWE-400 | many/large cookies per request |
| HTTP-212 | Header count amplification | CWE-400 | thousands of headers |

### Path / traversal (documented engines)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-213 | Apache 2.4.49 path traversal | CVE-2021-41773 | `%2e%2e` traversal/RCE |
| HTTP-214 | Apache 2.4.50 bypass | CVE-2021-42013 | double-encoded follow-up |
| HTTP-215 | IIS Unicode traversal | CVE-2000-0884 | `%c0%af` overlong slash |
| HTTP-216 | nginx alias traversal | CWE-22 | misconfigured `alias` off-by-slash |
| HTTP-217 | nginx merge_slashes off | CWE-22 | `//` collapses past a guard |
| HTTP-218 | Tomcat AJP Ghostcat | CVE-2020-1938 | file read/inclusion via AJP |
| HTTP-219 | Spring path-pattern bypass | CWE-22 | `..;/` matrix-segment traversal |
| HTTP-220 | Encoded-slash in path | CWE-22 | `%2f` decoded after authz |
| HTTP-221 | Semicolon path param (Tomcat) | CWE-20 | `;jsessionid` segment confusion |
| HTTP-222 | Trailing-dot host/path (Windows) | CWE-289 | `index.jsp.` source disclosure |

### Auth / session / JWT (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-223 | JWT `alg` array | CWE-347 | `alg` as an array confuses validation |
| HTTP-224 | JWT JSON duplicate `alg` | CWE-347 | duplicate header keys |
| HTTP-225 | JWT psychic signatures | CVE-2022-21449 | ECDSA `(0,0)` signature (lib-class) |
| HTTP-226 | JWT key-id SQL/LDAP | CWE-89 | `kid` injection into a lookup |
| HTTP-227 | JWT billion-hashes (PBKDF) | CWE-400 | costly `p2c` in JWE/JWK |
| HTTP-228 | Session puzzling | CWE-841 | a variable set in one flow trusted in another |
| HTTP-229 | Cookie `__Host-` bypass | CWE-565 | crafted prefix defeats the check |
| HTTP-230 | Cookie sandwich/tossing | CWE-784 | duplicate cookies pick the attacker's |
| HTTP-231 | Cross-site cookie shadowing | CWE-565 | sibling subdomain shadows the cookie |
| HTTP-232 | OAuth/OIDC state CSRF | CWE-352 | missing `state` in a callback |
| HTTP-233 | JWT none via mixed case nested | CWE-347 | `nOnE` after a JSON unescape |

### Injection contexts (second-order, documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-234 | Second-order XSS | CWE-79 | stored value rendered later |
| HTTP-235 | DOM/reflected via JSON response | CWE-79 | JSON reflected into HTML |
| HTTP-236 | Content-Type sniff XSS | CWE-430 | text served, browser runs HTML |
| HTTP-237 | SVG/XML upload XSS | CWE-79 | served SVG executes script |
| HTTP-238 | Reflected file download | CWE-binary | attacker-named download runs |
| HTTP-239 | CSV/formula injection in export | CWE-1236 | `=cmd` in an exported field |
| HTTP-240 | NoSQL injection (app) | CWE-943 | operator injection in a query object |
| HTTP-241 | LDAP injection (app) | CWE-90 | filter metacharacters |
| HTTP-242 | XPath injection (app) | CWE-643 | query metacharacters |
| HTTP-243 | Email/SMTP header injection | CWE-93 | CRLF in an email field |
| HTTP-244 | Log4Shell-style lookup | CVE-2021-44228 | `${jndi:...}` reaches a logger/evaluator |
| HTTP-245 | SSTI to RCE | CWE-1336 | template expression executes |

### CORS / WebSocket / headers (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-246 | CORS `Origin` regex dot bypass | CWE-942 | `.` in a regex matches any char |
| HTTP-247 | CORS scheme/port ignored | CWE-942 | http vs https or port not checked |
| HTTP-248 | CORS post-domain wildcard | CWE-942 | `trusted.attacker.com` |
| HTTP-249 | CSWSH via cookie auth | CWE-1385 | cross-site WS rides cookies |
| HTTP-250 | WebSocket smuggling | CWE-444 | fake 101 upgrade tunnels requests |
| HTTP-251 | WS compression bomb | CVE-class | permessage-deflate expansion |
| HTTP-252 | Missing `X-Frame-Options`/frame-ancestors | CWE-1021 | clickjacking |
| HTTP-253 | Dangling-markup injection | CWE-79 | unterminated attribute exfiltrates |
| HTTP-254 | Reverse tabnabbing | CWE-1022 | `target=_blank` without noopener |
| HTTP-255 | Mixed-content downgrade | CWE-319 | http subresource on an https page |

### Business logic / misc (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-256 | Race-condition limit bypass | CWE-362 | parallel requests double-spend (TOCTOU) |
| HTTP-257 | Mass-assignment privilege | CWE-915 | extra `is_admin` field bound |
| HTTP-258 | IDOR on numeric id | CWE-639 | increment id to reach others |
| HTTP-259 | GUID/UUID predictability | CWE-340 | guessable v1 UUIDs |
| HTTP-260 | Password-reset host poisoning | CWE-640 | `Host` poisons the reset link |
| HTTP-261 | Open redirect → token theft | CWE-601 | OAuth token leaked via redirect |
| HTTP-262 | Rate-limit reset via header | CWE-348 | spoofed `X-Forwarded-For` resets buckets |
| HTTP-263 | Email enumeration | CWE-204 | response/timing differs for valid users |
| HTTP-264 | Username enumeration via timing | CWE-208 | login timing leaks validity |
| HTTP-265 | Verbose error stack leak | CWE-209 | a 500 exposes internals |
| HTTP-266 | Debug endpoint exposed | CWE-489 | a left-on debug/admin route |
| HTTP-267 | Directory indexing leak | CWE-548 | auto-index reveals files |
| HTTP-268 | Backup/temp file exposure | CWE-530 | `.bak`/`~`/`.swp` served |
| HTTP-269 | Source disclosure via encoding | CWE-540 | `%00`/null served raw source |
| HTTP-270 | Method override abuse | CWE-650 | `X-HTTP-Method-Override` bypasses authz |

### Protocol & state (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-271 | Connection reuse after auth | CWE-441 | a pooled conn keeps another user's identity |
| HTTP-272 | TLS session resumption confusion | CWE-441 | resumed session crosses identities |
| HTTP-273 | Early-data (0-RTT) replay | CWE-294 | TLS 1.3 0-RTT request replay |
| HTTP-274 | ALPN/protocol confusion | CWE-436 | negotiated protocol mismatch |
| HTTP-275 | Trailer header smuggling | CWE-444 | chunked trailers smuggle headers |
| HTTP-276 | Absolute-URI vs Host mismatch | CWE-444 | request-target vs Host disagree |
| HTTP-277 | Pipelining response queue poisoning | CWE-444 | queued responses misaligned |
| HTTP-278 | Keep-alive timeout race | CWE-362 | response written to a reused socket |
| HTTP-279 | 100-continue body handling | CWE-444 | body sent before/after continue |
| HTTP-280 | Invalid chunk-size hex | CWE-444 | non-hex/oversized chunk length |
| HTTP-281 | Negative/huge Content-Length | CWE-190 | CL parsing overflow |
| HTTP-282 | Duplicate Host headers | CWE-444 | two `Host` values disagree |
| HTTP-283 | Whitespace before colon | CWE-444 | `Header : value` parsing variance |

---

## Round 3 — deeper / documented

### Smuggling & connection-level (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-284 | Visible normalization desync | CWE-444 | edge normalizes a header the origin keeps |
| HTTP-285 | Header-name case desync | CWE-444 | `Transfer-Encoding` vs `transfer-encoding` |
| HTTP-286 | Tab-as-space TE obfuscation | CWE-444 | `Transfer-Encoding:\tchunked` |
| HTTP-287 | Vertical-tab/FF in headers | CWE-444 | exotic whitespace splits parsers |
| HTTP-288 | Double Transfer-Encoding | CWE-444 | two TE headers disagree |
| HTTP-289 | Chunk extension smuggling | CWE-444 | `1;ext=...` chunk metadata |
| HTTP-290 | Last-chunk trailer injection | CWE-444 | headers after `0\r\n` |
| HTTP-291 | Connection: keep-alive desync | CWE-444 | hop-by-hop directive abuse |
| HTTP-292 | Pipelined-request boundary | CWE-444 | second request consumed as body |
| HTTP-293 | HTTP/1.0 with Keep-Alive | CWE-444 | version/keep-alive mismatch |

### Cache & proxy (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-294 | Unkeyed cookie cache poison | CWE-525 | cookie reflected into a cached page |
| HTTP-295 | Cache-key path-confusion | CWE-436 | `;`/`%2f` differs from the cache key |
| HTTP-296 | Cache poisoning via 404 | CWE-525 | cached error reflects input |
| HTTP-297 | Range-cache poisoning | CWE-525 | partial response cached as full |
| HTTP-298 | Vary header omission | CWE-525 | content-negotiated response mis-cached |
| HTTP-299 | CDN-origin header trust | CWE-444 | origin trusts a forwarded header |
| HTTP-300 | Stale-while-revalidate abuse | CWE-525 | serving stale sensitive data |

### Auth / session / token (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-301 | JWT kid header path traversal | CWE-22 | `kid` reads `/dev/null` to force an empty key |
| HTTP-302 | JWT zip-bomb in JWE | CWE-409 | compressed JWE payload |
| HTTP-303 | JWT `b64:false` (RFC 7797) | CWE-347 | unencoded-payload option abuse |
| HTTP-304 | Bearer vs cookie precedence | CWE-287 | two credentials, wrong one trusted |
| HTTP-305 | Session id in URL | CWE-598 | id logged/leaked via Referer |
| HTTP-306 | Referer leaks token | CWE-200 | token in URL leaks cross-origin |
| HTTP-307 | Remember-me token theft | CWE-539 | long-lived token weakly protected |
| HTTP-308 | Step-up auth bypass | CWE-287 | sensitive action skips re-auth |
| HTTP-309 | Concurrent-login fixation | CWE-384 | parallel sessions share an id |
| HTTP-310 | CSRF token not rotated | CWE-352 | token reuse across privilege change |
| HTTP-311 | Double-submit subdomain inject | CWE-352 | sibling sets the csrf cookie |
| HTTP-312 | SameSite=Lax GET mutation | CWE-352 | top-level GET still sends the cookie |

### Injection contexts (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-313 | GraphQL injection | CWE-943 | query/operation injection |
| HTTP-314 | GraphQL batch/alias DoS | CWE-770 | aliased query amplification |
| HTTP-315 | GraphQL introspection leak | CWE-200 | schema exposed |
| HTTP-316 | XPath/XQuery injection | CWE-643 | query metacharacters |
| HTTP-317 | SSI injection | CWE-97 | server-side includes |
| HTTP-318 | ESI injection | CWE-74 | edge-side includes |
| HTTP-319 | CSV/TSV export formula | CWE-1236 | `=`/`@`/`+` cell prefix |
| HTTP-320 | Header value injection into log | CWE-117 | reflected to a log sink |
| HTTP-321 | Reflected content-type XSS | CWE-430 | echoed type drives execution |
| HTTP-322 | Polyglot upload (GIFAR) | CWE-434 | file valid as image and archive |
| HTTP-323 | SVG/HTML upload stored XSS | CWE-79 | served upload runs script |
| HTTP-324 | ZIP/path in multipart filename | CWE-22 | `../` in an upload filename |

### File / static (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-325 | If-Range cache bypass | RFC 9110 | mismatched validator returns full body |
| HTTP-326 | Multi-range memory amplification | CWE-400 | many small ranges |
| HTTP-327 | Range integer overflow | CWE-190 | start+length overflow |
| HTTP-328 | Conditional-request validator confusion | RFC 9110 | weak vs strong ETag |
| HTTP-329 | Symlinked publicDir root | CWE-59 | the root itself is a symlink |
| HTTP-330 | Case-insensitive dotfile bypass | CWE-178 | `.ENV` vs blocked `.env` |
| HTTP-331 | Encoded dotfile request | CWE-22 | `%2e%67it/config` |
| HTTP-332 | Index file shadowing | CWE-424 | uploaded `index.html` overrides |
| HTTP-333 | Content-Disposition RFC 6266 edge | CWE-116 | filename* encoding abuse |
| HTTP-334 | Large directory listing DoS | CWE-400 | huge folder render |

### DoS & resource (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-335 | ReDoS in route constraint | CWE-1333 | catastrophic `where()` regex |
| HTTP-336 | ReDoS in user pattern | CWE-1333 | app-supplied Lua pattern |
| HTTP-337 | Multipart part-count flood | CWE-400 | thousands of parts |
| HTTP-338 | Nested multipart | CWE-400 | multipart inside multipart |
| HTTP-339 | JSON key-count flood | CWE-407 | many object keys in a body |
| HTTP-340 | Query-parameter flood | CWE-400 | thousands of query params |
| HTTP-341 | Cookie-count flood | CWE-400 | many cookies per request |
| HTTP-342 | Header-line length flood | CWE-400 | one giant header line |
| HTTP-343 | Keep-alive socket hoarding | CWE-400 | idle connections held |
| HTTP-344 | Per-IP rate-limit spoof | CWE-348 | forged forwarded address |
| HTTP-345 | Rate-limit memory growth | CWE-400 | unbounded per-IP buckets |
| HTTP-346 | Async-handler leak under load | CWE-401 | coroutine refs accumulate |

### Headers / TLS / transport (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-347 | CSP bypass via JSONP/nonce reuse | CWE-1021 | weak/static CSP nonce |
| HTTP-348 | CSP unsafe-inline/eval | CWE-1021 | permissive policy |
| HTTP-349 | HSTS not preloaded | CWE-319 | first-visit downgrade |
| HTTP-350 | Permissions-Policy missing | CWE-693 | powerful features ungated |
| HTTP-351 | Referrer-Policy leak | CWE-200 | full URL leaked cross-origin |
| HTTP-352 | X-Frame vs frame-ancestors gap | CWE-1021 | clickjacking via legacy gap |
| HTTP-353 | TLS session-ticket key rotation | CWE-323 | static ticket key |
| HTTP-354 | OCSP stapling absent | CWE-299 | revocation not advertised |
| HTTP-355 | ALPN downgrade | CWE-757 | protocol negotiation tampering |
| HTTP-356 | 0-RTT early-data on mutating route | CWE-294 | replayable early data |
| HTTP-357 | Mixed HTTP/HTTPS listener confusion | CWE-319 | plaintext sibling listener |

### WebSocket & streaming (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-358 | WS subprotocol confusion | CWE-436 | negotiated subprotocol mismatch |
| HTTP-359 | WS extension negotiation abuse | CWE-409 | permessage-deflate bomb |
| HTTP-360 | WS masking not enforced | CWE-20 | unmasked client frame accepted |
| HTTP-361 | WS fragmented control frame | CWE-20 | control frame fragmentation |
| HTTP-362 | WS reserved-bit handling | CWE-20 | RSV bits set without extension |
| HTTP-363 | WS close-code abuse | CWE-20 | invalid close codes |
| HTTP-364 | WS UTF-8 validation (text frame) | CWE-176 | invalid UTF-8 in a text frame |
| HTTP-365 | WS ping/pong flood | CWE-400 | control-frame flooding |
| HTTP-366 | Chunked-stream backpressure | CWE-400 | slow client stalls a streamed response |
| HTTP-367 | SSE connection hoarding | CWE-400 | many event-stream connections |

### Business logic & API (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-368 | TOCTOU on balance/quota | CWE-367 | parallel spend race |
| HTTP-369 | Idempotency-key bypass | CWE-696 | duplicate side effects |
| HTTP-370 | Negative-quantity order | CWE-840 | negative value flips logic |
| HTTP-371 | Price/parameter tampering | CWE-472 | client-controlled price field |
| HTTP-372 | Coupon/replay abuse | CWE-837 | reuse a single-use token |
| HTTP-373 | Workflow step skipping | CWE-840 | jump past a required step |
| HTTP-374 | Excessive data exposure | CWE-213 (API3) | endpoint returns extra fields |
| HTTP-375 | Unrestricted business flow | API6 | automation abuses a sensitive flow |
| HTTP-376 | Improper inventory (shadow API) | API9 | undocumented/old version reachable |
| HTTP-377 | Unsafe API consumption | API10 | trusting a third-party response |
| HTTP-378 | Pagination resource abuse | CWE-770 | huge page-size requests |
| HTTP-379 | Sort/filter injection | CWE-89 | order-by from input |

### Misc / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| HTTP-380 | Request-line fuzz | CWE-20 | malformed method/target/version |
| HTTP-381 | Header-block fuzz | CWE-20 | malformed/huge header sets |
| HTTP-382 | Body-grammar fuzz (json/form/multipart) | CWE-20 | structure-aware body fuzzing |
| HTTP-383 | Differential vs reference server | CWE-697 | parsing diverges from a known server |
