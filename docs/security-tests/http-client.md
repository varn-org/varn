# 📡 http — client

The client call is `http.client.request{ url, method, headers, body, timeoutSeconds, verifyTls, insecure, maxResponseBytes }`.

### SSRF — targets

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-001 | Loopback IPv4 | CWE-918 | A request to `http://127.0.0.1/` reaches local services |
| CLI-002 | Loopback range | CWE-918 | The range `http://127.0.0.2`…`127.255.255.255` |
| CLI-003 | Loopback IPv6 | CWE-918 | The URL `http://[::1]/` |
| CLI-004 | `0.0.0.0` / `[::]` | CWE-918 | A wildcard address reaches local listeners |
| CLI-005 | AWS/GCP/Azure IMDS | CWE-918 | The metadata at `http://169.254.169.254/` |
| CLI-006 | IMDS IPv6 | CWE-918 | The URL `http://[fd00:ec2::254]/` |
| CLI-007 | ECS task metadata | CWE-918 | The URL `http://169.254.170.2/` |
| CLI-008 | Alibaba/Oracle metadata | CWE-918 | The addresses `100.100.100.200` / `192.0.0.192` |
| CLI-009 | Link-local | CWE-918 | The range `169.254.0.0/16` |
| CLI-010 | RFC1918 private ranges | CWE-918 | The ranges `10/8`, `172.16/12`, `192.168/16` |
| CLI-011 | CGNAT range | CWE-918 | The range `100.64.0.0/10` |
| CLI-012 | `.internal`/`.local` names | CWE-918 | Internal DNS suffixes |
| CLI-013 | UNIX domain / abstract | CWE-918 | Scheme/host tricks to a local socket |

### SSRF — filter bypass encodings

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-014 | Decimal IP | CWE-918 | The URL `http://2130706433/` = `127.0.0.1` |
| CLI-015 | Octal IP | CWE-918 | The URL `http://0177.0.0.1/` |
| CLI-016 | Hex IP | CWE-918 | The URL `http://0x7f000001/` |
| CLI-017 | Mixed/short IP forms | CWE-918 | The forms `127.1`, `127.0.1` |
| CLI-018 | IPv4-mapped IPv6 | CWE-918 | The address `[::ffff:127.0.0.1]` |
| CLI-019 | IPv6 zone id | CWE-918 | The address `[fe80::1%25eth0]` |
| CLI-020 | Userinfo `@` trick | CWE-918 | The URL `http://expected@127.0.0.1/` |
| CLI-021 | Userinfo with credentials | CWE-918 | A parser split on `http://127.0.0.1:80\@evil/` |
| CLI-022 | URL parser confusion | CWE-918/436 | Host differs between validator and client |
| CLI-023 | Unicode/IDN homograph host | CWE-918 | Punycode/confusable hostnames |
| CLI-024 | Trailing dot / case host | CWE-918 | The host `127.0.0.1.` / case tricks |
| CLI-025 | Enclosed-alphanumeric digits | CWE-918 | Full-width/circled digits in host |

### SSRF — redirect & DNS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-026 | Redirect to internal | CWE-601/918 | A 30x `Location` points at an internal target |
| CLI-027 | Redirect to file scheme | CWE-918 | A 30x `Location: file:///etc/passwd` |
| CLI-028 | Redirect chain depth | CWE-400 | Unbounded redirect chain ties up the client |
| CLI-029 | DNS rebinding | CWE-918 | Safe at resolve time, internal at connect time |
| CLI-030 | TOCTOU host resolution | CWE-367 | Validation resolves a different IP than the request |
| CLI-031 | DNS pinning absent | CWE-918 | Each lookup may return a different IP |

### Scheme & protocol

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-032 | `file://` | CWE-918 | Local file read |
| CLI-033 | `gopher://` | CWE-918 | Crafted bytes to arbitrary TCP service |
| CLI-034 | `ftp://`/`dict://`/`ldap://` | CWE-918 | Other-protocol smuggling |
| CLI-035 | Scheme case/whitespace | CWE-20 | Forms like `HtTp:`/leading spaces bypass a scheme check |
| CLI-036 | Missing-scheme URL | CWE-20 | Relative-scheme handling of `//host/path` |
| CLI-037 | Non-default port to internal | CWE-918 | Internal admin ports (6379, 9200, …) |

### TLS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-038 | No certificate verification | CWE-295 | Accepting any cert enables MITM |
| CLI-039 | Hostname not checked | CWE-297 | Cert CN/SAN mismatch accepted |
| CLI-040 | Self-signed accepted | CWE-295 | Untrusted chain accepted by default |
| CLI-041 | Expired cert accepted | CWE-298 | Past-validity cert accepted |
| CLI-042 | Revoked cert (no OCSP/CRL) | CWE-299 | Revoked cert still accepted |
| CLI-043 | Weak protocol/cipher to server | CWE-326/327 | Client offers legacy protocol/cipher |
| CLI-044 | `insecure` flag misuse | CWE-295 | Opt-out left on in production |
| CLI-045 | TLS downgrade by MITM | CWE-757 | Forced weaker negotiation |

### Request injection & headers

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-046 | Header CRLF injection | CWE-113 | A `\r\n` in a client header splits the request |
| CLI-047 | Header name token violation | CWE-20 | Controls/space/`:` in a header name |
| CLI-048 | Smuggling via CL/TE in request | CWE-444 | Conflicting CL/TE the client sets |
| CLI-049 | Method injection | CWE-74 | CRLF/space in the method |
| CLI-050 | URL path/query injection | CWE-113 | CRLF in path/query reaches the request line |
| CLI-051 | Host header override | CWE-644 | Attacker-set `Host` for routing/cache |
| CLI-052 | Cookie/auth header leak on redirect | CWE-200 | Sensitive headers re-sent cross-origin on 30x |

### Response handling & DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-053 | Unbounded response body | CWE-400 | Server streams an unbounded body into memory |
| CLI-054 | `maxResponseBytes` off-by-one | CWE-193 | Boundary at exactly the cap |
| CLI-055 | Decompression bomb | CWE-409 | A gzip/deflate response expands hugely |
| CLI-056 | Chunked response bomb | CWE-400 | Huge/again-chunked response |
| CLI-057 | Slow response (read) | CWE-400 | Server drip-feeds to hold the worker |
| CLI-058 | No/!enforced timeout | CWE-400 | Blackhole server never returns |
| CLI-059 | Huge/abundant response headers | CWE-400 | Header flood exhausts the parser |
| CLI-060 | Status-line/version fuzz | CWE-20 | Malformed status line mis-parsed |
| CLI-061 | Response framing injection | CWE-74 | Response headers or body spoof the HTTP/1.1 framing |
| CLI-062 | Content-Length mismatch | CWE-444 | Declared vs actual body length |
| CLI-063 | Trailer-header abuse | CWE-444 | Chunked trailers smuggle headers |

### Memory / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-064 | Promise ref leak | CWE-401 | Request promise refs not released on error |
| CLI-065 | Cross-thread settle race | CWE-362 | Task-pool worker settles while loop runs |
| CLI-066 | Session/socket leak | CWE-404 | Client session/fd not closed on error |
| CLI-067 | Task-pool starvation | CWE-400 | Many slow requests exhaust the pool |
| CLI-068 | Body NUL safety | CWE-626 | Request body with NUL truncated |
| CLI-069 | Response NUL safety | CWE-626 | NUL in body truncates the returned string |
| CLI-070 | Header map injection (Lua) | CWE-20 | Non-string header keys/values |
| CLI-071 | Huge header table | CWE-400 | A million request headers |
| CLI-072 | Reentrant request in callback | CWE-674 | Issuing requests from a resolve callback |
| CLI-073 | URL fuzz | CWE-20 | Malformed/garbage URLs crash the parser |
| CLI-074 | Timeout integer overflow | CWE-190 | A `timeoutSeconds` near `INT_MAX` |
| CLI-075 | Negative/zero timeout | CWE-20 | Handling of `timeoutSeconds <= 0` |
| CLI-076 | `maxResponseBytes` overflow | CWE-190 | Huge cap value wraps the size type |
| CLI-077 | Concurrent identical requests | CWE-362 | Shared static TLS context race |
| CLI-078 | Emscripten/native parity | CWE-697 | Browser-fetch path differs from native checks |

### Protocol, auth & misc

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-079 | Keep-alive connection reuse | CWE-441 | A pooled connection reused across hosts/identities |
| CLI-080 | Connection-reuse identity mix | CWE-664 | Response routed to the wrong request on reuse |
| CLI-081 | Proxy honored from env | CWE-441 | The `*_proxy` env redirects traffic |
| CLI-082 | Basic-auth in URL leaks | CWE-522 | A `user:pass@host` logged/forwarded |
| CLI-083 | Auth re-sent cross-origin | CWE-200 | The `Authorization` header re-sent after a redirect |
| CLI-084 | Accept-Encoding tampering | CWE-444 | Forced encoding triggers a decode bomb |
| CLI-085 | Response charset confusion | CWE-176 | Declared charset != body bytes |
| CLI-086 | Cookie store cross-host | CWE-565 | Cookies sent to the wrong host |
| CLI-087 | Port 0 / out-of-range port | CWE-20 | A port `:0` or `:99999` mishandled |
| CLI-088 | IPv6 literal bracket parse | CWE-20 | Parsing edge cases of `[::1]:8080` |
| CLI-089 | Empty/whitespace URL | CWE-20 | Input `""` / spaces crash the parser |
| CLI-090 | Extremely long URL | CWE-400 | Multi-MB URL exhausts memory |
| CLI-091 | Method case/verb tampering | CWE-20 | Lowercase/unknown verbs |
| CLI-092 | TRACE/CONNECT enabled | CWE-16 | Dangerous methods reach the wire |
| CLI-093 | Expect/100-continue handling | CWE-400 | Continue flow hangs the client |
| CLI-094 | Retry amplification | CWE-400 | Automatic retries multiply load |
| CLI-095 | Idempotency on retry | CWE-696 | Retried non-idempotent request duplicates side effects |
| CLI-096 | Body streaming backpressure | CWE-400 | Large request body buffered fully |
| CLI-097 | Concurrent TLS init race | CWE-362 | First-use SSL manager init races |
| CLI-098 | Error message info leak | CWE-209 | Internal error text returned to Lua |
| CLI-099 | Content-Length integer parse | CWE-190 | Crafted response Content-Length overflows |
| CLI-100 | Non-UTF8 header values | CWE-176 | Raw bytes in response headers |

---

## Additional cases (documented attacks & CVEs)

### SSRF — documented techniques & targets

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-101 | Capital One-style IMDS theft | CWE-918 | SSRF → IMDSv1 creds (2019 breach pattern) |
| CLI-102 | IMDSv2 token bypass attempt | CWE-918 | Reaching the PUT token endpoint via SSRF |
| CLI-103 | GCP metadata `Metadata-Flavor` | CWE-918 | Header-gated metadata still reachable |
| CLI-104 | Azure IMDS `Metadata: true` | CWE-918 | Azure metadata endpoint |
| CLI-105 | Kubernetes API/service IP | CWE-918 | The targets `10.0.0.1`/`kubernetes.default` |
| CLI-106 | Docker socket via TCP | CWE-918 | The `:2375` daemon reachable |
| CLI-107 | Redis/Memcached SSRF | CWE-918 | A `gopher://`-style or inline command |
| CLI-108 | Elasticsearch/Solr SSRF | CWE-918 | Internal `:9200`/`:8983` |
| CLI-109 | Internal admin panel reach | CWE-918 | The `localhost:` dashboards |
| CLI-110 | Blind SSRF (timing/OOB) | CWE-918 | Confirm via timing or DNS callback |
| CLI-111 | SSRF to cloud function/metadata IPv6 | CWE-918 | The address `[fd00:ec2::254]` |
| CLI-112 | PDF/SSRF via fetched resource | CWE-918 | Server fetches an attacker URL |

### SSRF — parser/encoding bypass (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-113 | Orange Tsai URL parser confusion | CWE-918 | The "A New Era of SSRF" parser differential |
| CLI-114 | `http://a@b@c/` double-at | CWE-918 | Ambiguous userinfo |
| CLI-115 | `http://a#@b/` fragment trick | CWE-918 | Fragment hides the real host |
| CLI-116 | Backslash host split | CWE-918 | The URL `http://expected\@evil/` |
| CLI-117 | Whitespace/tab in URL | CWE-918 | Embedded control chars |
| CLI-118 | CR/LF in URL host | CWE-113 | Host with line breaks |
| CLI-119 | Percent-encoded host | CWE-918 | The host `127%2e0%2e0%2e1` |
| CLI-120 | Dotless decimal + path | CWE-918 | The URL `http://2130706433:80/x` |
| CLI-121 | IPv6 scoped/embedded IPv4 | CWE-918 | The address `[::ffff:7f00:1]` |
| CLI-122 | IDNA/punycode host | CWE-918 | A Unicode host normalizes internally |
| CLI-123 | Enclosing-bracket trick | CWE-918 | Malformed `[host]` parsing |
| CLI-124 | Trailing-dot FQDN | CWE-918 | The host `localhost. ` bypasses an allowlist |

### Redirect / DNS (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-125 | 30x to `file://` | CWE-918 | Redirect changes scheme to file |
| CLI-126 | 30x to internal after allow | CWE-918 | First hop allowed, redirect internal |
| CLI-127 | Open-redirect chain to metadata | CWE-918 | An `r3dir`-style chain to IMDS |
| CLI-128 | DNS rebinding (TTL 0) | CWE-918 | Rebind between check and connect |
| CLI-129 | DNS A/AAAA split-horizon | CWE-918 | IPv4 safe, IPv6 internal |
| CLI-130 | Multiple A records | CWE-918 | One safe, one internal |
| CLI-131 | DNS pinning absent across redirects | CWE-918 | Re-resolution per hop |
| CLI-132 | CRLF in redirect Location | CWE-113 | Header injection on follow |

### TLS / certificate (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-133 | NULL-byte CN | CVE-2009-2408 | The cert CN `evil.com\0.good.com` |
| CLI-134 | Wildcard cert over-match | CWE-295 | A `*.com`/multi-label wildcard |
| CLI-135 | Hostname-verify disabled by default | CWE-295 | Verify mode left relaxed |
| CLI-136 | Missing SAN check (CN-only) | CWE-297 | Trusting CN, ignoring SAN |
| CLI-137 | Chain-of-trust not built | CWE-295 | Intermediate not verified |
| CLI-138 | Basic-constraints not checked | CWE-295 | A leaf used as a CA |
| CLI-139 | Self-signed accepted (handler) | CWE-295 | Accept-all certificate handler |
| CLI-140 | Renegotiation/`CVE-2009-3555` | CWE-300 | Client reneg injection |
| CLI-141 | TLS compression (CRIME) | CVE-2012-4929 | Compression side-channel |
| CLI-142 | Downgrade to TLS 1.0 | CWE-757 | MITM forces legacy protocol |
| CLI-143 | OCSP/CRL not consulted | CWE-299 | Revoked cert accepted |
| CLI-144 | OpenSSL lib CVE surface | CWE-1395 | Unpatched client TLS lib |

### Library/CVE-class & response

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-145 | curl-style URL glob/control | CWE-20 | Unexpected URL features |
| CLI-146 | Header-folding (obs-fold) | CWE-444 | Obsolete line folding in response |
| CLI-147 | Response splitting via redirect | CWE-113 | CRLF in a redirected request |
| CLI-148 | gzip bomb response | CWE-409 | Small body, huge inflate |
| CLI-149 | brotli/deflate bomb | CWE-409 | Other content-encodings |
| CLI-150 | Nested content-encoding | CWE-409 | The header `Content-Encoding: gzip, gzip` |
| CLI-151 | Chunk-ext flood in response | CWE-400 | Huge chunk extensions |
| CLI-152 | Trailer injection in response | CWE-444 | Response trailers add headers |
| CLI-153 | Content-Length vs body mismatch | CWE-444 | Client trusts CL over actual |
| CLI-154 | Status-line smuggling | CWE-444 | Malformed status line |
| CLI-155 | Set-Cookie injection on redirect | CWE-565 | Response sets cookies cross-host |
| CLI-156 | Compression-ratio guard absent | CWE-409 | No decoded-size cap |

### Request injection & header (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-157 | CRLF header injection (curl-class) | CWE-93 | Newline in a custom header |
| CLI-158 | Host header override to bypass | CWE-444 | Client-set Host differs from URL |
| CLI-159 | Smuggling via client CL/TE | CWE-444 | Conflicting framing the client adds |
| CLI-160 | Authorization leak on cross-host redirect | CWE-200 | Credentials re-sent (curl CVE-class) |
| CLI-161 | Cookie leak on cross-host redirect | CWE-200 | Cookies re-sent to a new host |
| CLI-162 | Proxy header injection | CWE-441 | The `Proxy-*` headers honored |
| CLI-163 | `Expect: 100-continue` mishandle | CWE-444 | Body/continue desync |
| CLI-164 | TE in outbound request | CWE-444 | Client emits chunked unexpectedly |
| CLI-165 | Method spoofing (CONNECT) | CWE-16 | Tunneling via `CONNECT` |

### State, concurrency, resource (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-166 | Connection-pool identity confusion | CWE-441 | Response for the wrong request |
| CLI-167 | TLS-session-resumption cross-host | CWE-441 | Resumed session reused wrongly |
| CLI-168 | 0-RTT early-data replay | CWE-294 | TLS 1.3 early data replayed |
| CLI-169 | Keep-alive race on reuse | CWE-362 | Reused socket mid-response |
| CLI-170 | Unbounded redirect loop | CWE-835 | A→B→A redirect cycle |
| CLI-171 | Retry storm amplification | CWE-405 | Retries multiply against a target |
| CLI-172 | DNS resolution DoS | CWE-400 | Slow/large DNS responses |
| CLI-173 | Connect to closed port flood | CWE-400 | Port-scan-as-DoS |
| CLI-174 | Memory per concurrent request | CWE-400 | Many in-flight responses buffered |
| CLI-175 | Slow-DNS / slow-connect hang | CWE-400 | No connect timeout |
| CLI-176 | First-use TLS init race | CWE-362 | SSL manager initialized concurrently |
| CLI-177 | Socket fd leak on error | CWE-404 | Session not closed on failure |

### Misc / protocol / fuzz (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-178 | HTTP/0.9 response handling | CWE-444 | Bodyless legacy response |
| CLI-179 | Mixed HTTP version response | CWE-436 | Version downgrade in reply |
| CLI-180 | UTF-7/charset response confusion | CWE-176 | Charset-driven misinterpretation |
| CLI-181 | BOM in response body | CWE-176 | Leading BOM mishandled |
| CLI-182 | Huge header value | CWE-400 | Multi-MB single header |
| CLI-183 | Duplicate Content-Length (response) | CWE-444 | Two CL values |
| CLI-184 | Negative/overflow CL (response) | CWE-190 | CL parse overflow |
| CLI-185 | URL with embedded credentials logged | CWE-532 | A `user:pass@` written to logs |
| CLI-186 | Error message reveals target IP | CWE-209 | Internal IP in an error |
| CLI-187 | Timing reveals internal reachability | CWE-204 | Blind-SSRF confirmation |
| CLI-188 | Response-size oracle | CWE-204 | Length differences leak state |
| CLI-189 | Differential dummy/native client | CWE-697 | The wasm-fetch and native checks diverge |
| CLI-190 | Content-Length vs chunked spoof | CWE-444 | Response forges Content-Length against chunked framing |
| CLI-191 | NUL in URL | CWE-626 | URL truncation at NUL |
| CLI-192 | Extremely long URL | CWE-400 | Multi-MB URL |
| CLI-193 | Unicode normalization of URL | CWE-178 | Host normalized after a check |
| CLI-194 | Port 0 / overflow port | CWE-190 | Invalid port handling |
| CLI-195 | IPv6 zone-id injection | CWE-918 | A `%25` zone id in host |
| CLI-196 | Scheme-relative `//host` | CWE-918 | Protocol inherited unexpectedly |
| CLI-197 | `data:`/`blob:` scheme | CWE-918 | Non-network scheme handling |
| CLI-198 | TRACE method reflection | CWE-16 | XST-style reflection |
| CLI-199 | Idempotency on auto-retry | CWE-696 | Retried POST duplicates effects |
| CLI-200 | Fuzz URL+headers+body | CWE-20 | Coverage-guided fuzz never crashes |

---

## Round 3 — deeper / documented

### SSRF gadgets & targets (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-201 | `gopher://` to SMTP/Redis | CWE-918 | Crafted multi-line payload to a TCP service |
| CLI-202 | `dict://` service probe | CWE-918 | Banner-grab internal services |
| CLI-203 | ftp:// active-mode pivot | CWE-918 | FTP command channel abuse |
| CLI-204 | tftp:// internal read | CWE-918 | UDP file fetch |
| CLI-205 | sftp/ssh probe | CWE-918 | Internal port reachability |
| CLI-206 | Consul/etcd API reach | CWE-918 | The `:8500`/`:2379` config stores |
| CLI-207 | Vault API reach | CWE-918 | The `:8200` secret store |
| CLI-208 | Prometheus/Grafana reach | CWE-918 | Internal dashboards |
| CLI-209 | Internal Git/CI reach | CWE-918 | The `:3000`/`:8080` services |
| CLI-210 | Cloud SQL proxy reach | CWE-918 | Local DB proxy |
| CLI-211 | Link-local IPv6 metadata | CWE-918 | The addresses `[fe80::]`/`[fd00:ec2::254]` |
| CLI-212 | Carrier-grade NAT range | CWE-918 | The range `100.64.0.0/10` |
| CLI-213 | Broadcast/multicast target | CWE-406 | Sending to a group address |

### SSRF parser bypass (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-214 | Mixed octal/decimal/hex octets | CWE-918 | The `0x7f.0.0.1` style |
| CLI-215 | Dword + path | CWE-918 | The URL `http://2130706433/admin` |
| CLI-216 | Zero-padded octets | CWE-918 | The host `127.000.000.001` |
| CLI-217 | Fullwidth/IDN digits | CWE-918 | A Unicode-confusable host |
| CLI-218 | Tab/newline in URL | CWE-113 | Control bytes split parsing |
| CLI-219 | Backslash path/host split | CWE-918 | The URL `http://good\@evil` |
| CLI-220 | Fragment-hidden host | CWE-918 | The URL `http://evil#@good` |
| CLI-221 | Userinfo with percent-encoding | CWE-918 | The sequence `%40` decodes to `@` |
| CLI-222 | Trailing-dot allowlist bypass | CWE-918 | The host `internal.host. ` |
| CLI-223 | Case-only allowlist bypass | CWE-178 | Host case differs from the rule |
| CLI-224 | Validator vs client URL parse | CWE-436 | The two disagree on the host |
| CLI-225 | Double-encoded host | CWE-918 | The `%2569` style |

### Redirect / DNS (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-226 | Redirect scheme change to gopher | CWE-918 | A 30x switches protocol |
| CLI-227 | Redirect host re-validation absent | CWE-918 | Only the first hop checked |
| CLI-228 | Cross-protocol redirect (https→http) | CWE-319 | Downgrade on follow |
| CLI-229 | Meta-refresh/JS not relevant (server fetch) | CWE-918 | Only HTTP 30x relevant |
| CLI-230 | DNS TTL-0 rebinding | CWE-918 | Re-resolve to internal |
| CLI-231 | DNS multi-record selection | CWE-918 | Pick an internal A/AAAA |
| CLI-232 | DNS pinning across redirects | CWE-918 | Re-resolution per hop |
| CLI-233 | Happy-eyeballs v4/v6 race | CWE-918 | An internal v6 address wins |

### TLS / certificate (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-234 | SAN wildcard over-match | CWE-295 | The pattern `*.example` matches too much |
| CLI-235 | IP-SAN vs DNS-SAN confusion | CWE-297 | Connecting by IP |
| CLI-236 | Internationalized-name match | CWE-297 | IDN cert name handling |
| CLI-237 | Trust-store override via env | CWE-295 | An `SSL_CERT_FILE` hijack |
| CLI-238 | Client-cert leakage | CWE-295 | Wrong client cert presented |
| CLI-239 | Session-ticket reuse cross-host | CWE-441 | Resumed to the wrong origin |
| CLI-240 | Renegotiation injection | CVE-2009-3555 | Client reneg |
| CLI-241 | ALPN mismatch | CWE-436 | Negotiated protocol wrong |
| CLI-242 | TLS 1.3 0-RTT replay | CWE-294 | Early-data on a retryable request |
| CLI-243 | Min-protocol not enforced | CWE-326 | Legacy TLS accepted |

### Response / encoding / DoS (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-244 | Content-Encoding chain bomb | CWE-409 | Stacking `gzip, gzip, br` |
| CLI-245 | Transfer + Content-Encoding combo | CWE-444 | TE/CE interaction |
| CLI-246 | Decoded-size cap absent | CWE-409 | Cap applies to compressed only |
| CLI-247 | Obs-fold header in response | CWE-444 | Line folding revival |
| CLI-248 | Status-line CRLF injection | CWE-113 | Malformed status reflected |
| CLI-249 | Header value with NUL | CWE-626 | NUL in a response header |
| CLI-250 | Charset-driven misinterpret | CWE-176 | UTF-7/UTF-16 response |
| CLI-251 | BOM-led body | CWE-176 | Leading BOM |
| CLI-252 | Trailer header override | CWE-444 | Trailers add/override headers |
| CLI-253 | Pipelined response misalignment | CWE-444 | Response queue poisoning |
| CLI-254 | Slow-trickle response | CWE-400 | Drip read holds the worker |
| CLI-255 | DNS-response DoS | CWE-400 | Huge/slow DNS reply |

### Request injection & headers (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-256 | Header name with CR | CWE-113 | Bare CR in a name |
| CLI-257 | Header value with LF | CWE-113 | Bare LF in a value |
| CLI-258 | Duplicate Host in request | CWE-444 | Two Host headers |
| CLI-259 | Authorization on redirect host | CWE-200 | Credentials re-sent (curl CVE class) |
| CLI-260 | Cookie sent to redirect host | CWE-200 | Cookie leaks cross-host |
| CLI-261 | Proxy-Authorization leak | CWE-522 | Proxy creds forwarded |
| CLI-262 | Method override smuggling | CWE-444 | Conflicting method/framing |
| CLI-263 | Expect/100-continue desync | CWE-444 | Body sent before continue |

### Connection / state / concurrency (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-264 | Pool identity confusion | CWE-441 | Reused connection crosses identities |
| CLI-265 | Keep-alive response misroute | CWE-664 | Response for a different request |
| CLI-266 | Connection reuse after error | CWE-664 | Tainted socket reused |
| CLI-267 | Redirect loop (no cap) | CWE-835 | A→B→A cycle |
| CLI-268 | Retry storm against a target | CWE-405 | Retries amplify load |
| CLI-269 | Concurrent-request fd leak | CWE-404 | Sessions not closed |
| CLI-270 | TLS init race (first use) | CWE-362 | SSL manager concurrency |
| CLI-271 | Reentrant request in callback | CWE-674 | Request from a resolve callback |
| CLI-272 | Cross-thread settle race | CWE-362 | Worker settles to the loop |
| CLI-273 | Promise leak on error | CWE-401 | Refs not released |

### Misc / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-274 | HTTP/0.9 response | CWE-444 | Bodyless legacy reply |
| CLI-275 | HTTP version downgrade reply | CWE-436 | Server claims a lower version |
| CLI-276 | `data:`/`blob:`/`javascript:` scheme | CWE-918 | Non-network scheme |
| CLI-277 | URL with embedded NUL | CWE-626 | Truncation at NUL |
| CLI-278 | URL fragment handling | CWE-20 | Fragment sent to the server |
| CLI-279 | Percent-encoding in path | CWE-113 | Encoded CRLF in the path |
| CLI-280 | Port out of range | CWE-190 | The port `:99999` |
| CLI-281 | IPv6 literal bracket fuzz | CWE-20 | Malformed `[..]` host |
| CLI-282 | Very long header value | CWE-400 | Multi-MB header |
| CLI-283 | Many request headers | CWE-400 | Header flood |
| CLI-284 | Timeout integer overflow | CWE-190 | A `timeoutSeconds` near limits |
| CLI-285 | `maxResponseBytes` overflow | CWE-190 | Huge cap wraps the size |
| CLI-286 | Negative/zero timeout | CWE-20 | Non-positive timeout |
| CLI-287 | Body NUL safety | CWE-626 | Request body with NUL |
| CLI-288 | Response NUL safety | CWE-626 | NUL in the returned body |
| CLI-289 | Header map non-string keys | CWE-20 | Non-string Lua header keys |
| CLI-290 | Content-Length parse overflow | CWE-190 | Crafted response Content-Length integer |
| CLI-291 | Error reveals resolved IP | CWE-209 | Internal IP in an error |
| CLI-292 | Timing confirms reachability | CWE-204 | Blind-SSRF oracle |
| CLI-293 | Response-size oracle | CWE-204 | Length leaks state |
| CLI-294 | Basic-auth in URL logged | CWE-532 | Credentials in logs |
| CLI-295 | Proxy from environment | CWE-441 | A `*_proxy` variable redirects traffic |
| CLI-296 | TRACE/XST reflection | CWE-16 | Method reflection |
| CLI-297 | Idempotency on POST retry | CWE-696 | Duplicated side effects |
| CLI-298 | wasm-fetch vs native parity | CWE-697 | Browser path skips checks |
| CLI-299 | ASan/UBSan trip on response parse | CWE-125 | Malformed response |
| CLI-300 | Differential vs reference client | CWE-697 | Behavior diverges from curl/libcurl |
