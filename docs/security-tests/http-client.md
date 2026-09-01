# 📡 http — client

`http.client.request{ url, method, headers, body, timeoutSeconds, verifyTls, insecure, maxResponseBytes }`.

### SSRF — targets

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-001 | Loopback IPv4 | CWE-918 | `http://127.0.0.1/` reaches local services |
| CLI-002 | Loopback range | CWE-918 | `http://127.0.0.2`…`127.255.255.255` |
| CLI-003 | Loopback IPv6 | CWE-918 | `http://[::1]/` |
| CLI-004 | `0.0.0.0` / `[::]` | CWE-918 | wildcard address reaches local listeners |
| CLI-005 | AWS/GCP/Azure IMDS | CWE-918 | `http://169.254.169.254/` metadata |
| CLI-006 | IMDS IPv6 | CWE-918 | `http://[fd00:ec2::254]/` |
| CLI-007 | ECS task metadata | CWE-918 | `http://169.254.170.2/` |
| CLI-008 | Alibaba/Oracle metadata | CWE-918 | `100.100.100.200` / `192.0.0.192` |
| CLI-009 | Link-local | CWE-918 | `169.254.0.0/16` |
| CLI-010 | RFC1918 private ranges | CWE-918 | `10/8`, `172.16/12`, `192.168/16` |
| CLI-011 | CGNAT range | CWE-918 | `100.64.0.0/10` |
| CLI-012 | `.internal`/`.local` names | CWE-918 | internal DNS suffixes |
| CLI-013 | UNIX domain / abstract | CWE-918 | scheme/host tricks to a local socket |

### SSRF — filter bypass encodings

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-014 | Decimal IP | CWE-918 | `http://2130706433/` = 127.0.0.1 |
| CLI-015 | Octal IP | CWE-918 | `http://0177.0.0.1/` |
| CLI-016 | Hex IP | CWE-918 | `http://0x7f000001/` |
| CLI-017 | Mixed/short IP forms | CWE-918 | `127.1`, `127.0.1` |
| CLI-018 | IPv4-mapped IPv6 | CWE-918 | `[::ffff:127.0.0.1]` |
| CLI-019 | IPv6 zone id | CWE-918 | `[fe80::1%25eth0]` |
| CLI-020 | Userinfo `@` trick | CWE-918 | `http://expected@127.0.0.1/` |
| CLI-021 | Userinfo with credentials | CWE-918 | `http://127.0.0.1:80\@evil/` parser split |
| CLI-022 | URL parser confusion | CWE-918/436 | host differs between validator and client |
| CLI-023 | Unicode/IDN homograph host | CWE-918 | punycode/confusable hostnames |
| CLI-024 | Trailing dot / case host | CWE-918 | `127.0.0.1.` / case tricks |
| CLI-025 | Enclosed-alphanumeric digits | CWE-918 | full-width/circled digits in host |

### SSRF — redirect & DNS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-026 | Redirect to internal | CWE-601/918 | 30x `Location` points at an internal target |
| CLI-027 | Redirect to file scheme | CWE-918 | 30x `Location: file:///etc/passwd` |
| CLI-028 | Redirect chain depth | CWE-400 | unbounded redirect chain ties up the client |
| CLI-029 | DNS rebinding | CWE-918 | safe at resolve time, internal at connect time |
| CLI-030 | TOCTOU host resolution | CWE-367 | validation resolves a different IP than the request |
| CLI-031 | DNS pinning absent | CWE-918 | each lookup may return a different IP |

### Scheme & protocol

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-032 | `file://` | CWE-918 | local file read |
| CLI-033 | `gopher://` | CWE-918 | crafted bytes to arbitrary TCP service |
| CLI-034 | `ftp://`/`dict://`/`ldap://` | CWE-918 | other-protocol smuggling |
| CLI-035 | Scheme case/whitespace | CWE-20 | `HtTp:`/leading spaces bypass a scheme check |
| CLI-036 | Missing-scheme URL | CWE-20 | `//host/path` relative-scheme handling |
| CLI-037 | Non-default port to internal | CWE-918 | internal admin ports (6379, 9200, …) |

### TLS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-038 | No certificate verification | CWE-295 | accepting any cert enables MITM |
| CLI-039 | Hostname not checked | CWE-297 | cert CN/SAN mismatch accepted |
| CLI-040 | Self-signed accepted | CWE-295 | untrusted chain accepted by default |
| CLI-041 | Expired cert accepted | CWE-298 | past-validity cert accepted |
| CLI-042 | Revoked cert (no OCSP/CRL) | CWE-299 | revoked cert still accepted |
| CLI-043 | Weak protocol/cipher to server | CWE-326/327 | client offers legacy protocol/cipher |
| CLI-044 | `insecure` flag misuse | CWE-295 | opt-out left on in production |
| CLI-045 | TLS downgrade by MITM | CWE-757 | forced weaker negotiation |

### Request injection & headers

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-046 | Header CRLF injection | CWE-113 | `\r\n` in a client header splits the request |
| CLI-047 | Header name token violation | CWE-20 | controls/space/`:` in a header name |
| CLI-048 | Smuggling via CL/TE in request | CWE-444 | conflicting CL/TE the client sets |
| CLI-049 | Method injection | CWE-74 | CRLF/space in the method |
| CLI-050 | URL path/query injection | CWE-113 | CRLF in path/query reaches the request line |
| CLI-051 | Host header override | CWE-644 | attacker-set `Host` for routing/cache |
| CLI-052 | Cookie/auth header leak on redirect | CWE-200 | sensitive headers re-sent cross-origin on 30x |

### Response handling & DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-053 | Unbounded response body | CWE-400 | server streams an unbounded body into memory |
| CLI-054 | `maxResponseBytes` off-by-one | CWE-193 | boundary at exactly the cap |
| CLI-055 | Decompression bomb | CWE-409 | gzip/deflate response expands hugely |
| CLI-056 | Chunked response bomb | CWE-400 | huge/again-chunked response |
| CLI-057 | Slow response (read) | CWE-400 | server drip-feeds to hold the worker |
| CLI-058 | No/!enforced timeout | CWE-400 | blackhole server never returns |
| CLI-059 | Huge/abundant response headers | CWE-400 | header flood exhausts the parser |
| CLI-060 | Status-line/version fuzz | CWE-20 | malformed status line mis-parsed |
| CLI-061 | Response framing injection | CWE-74 | response headers or body spoof the HTTP/1.1 framing |
| CLI-062 | Content-Length mismatch | CWE-444 | declared vs actual body length |
| CLI-063 | Trailer-header abuse | CWE-444 | chunked trailers smuggle headers |

### Memory / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-064 | Promise ref leak | CWE-401 | request promise refs not released on error |
| CLI-065 | Cross-thread settle race | CWE-362 | task-pool worker settles while loop runs |
| CLI-066 | Session/socket leak | CWE-404 | client session/fd not closed on error |
| CLI-067 | Task-pool starvation | CWE-400 | many slow requests exhaust the pool |
| CLI-068 | Body NUL safety | CWE-626 | request body with NUL truncated |
| CLI-069 | Response NUL safety | CWE-626 | NUL in body truncates the returned string |
| CLI-070 | Header map injection (Lua) | CWE-20 | non-string header keys/values |
| CLI-071 | Huge header table | CWE-400 | a million request headers |
| CLI-072 | Reentrant request in callback | CWE-674 | issuing requests from a resolve callback |
| CLI-073 | URL fuzz | CWE-20 | malformed/garbage URLs crash the parser |
| CLI-074 | Timeout integer overflow | CWE-190 | `timeoutSeconds` near INT_MAX |
| CLI-075 | Negative/zero timeout | CWE-20 | `timeoutSeconds <= 0` handling |
| CLI-076 | maxResponseBytes overflow | CWE-190 | huge cap value wraps the size type |
| CLI-077 | Concurrent identical requests | CWE-362 | shared static TLS context race |
| CLI-078 | Emscripten/native parity | CWE-697 | browser-fetch path differs from native checks |

### Protocol, auth & misc

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-079 | Keep-alive connection reuse | CWE-441 | a pooled connection reused across hosts/identities |
| CLI-080 | Connection-reuse identity mix | CWE-664 | response routed to the wrong request on reuse |
| CLI-081 | Proxy honored from env | CWE-441 | `*_proxy` env redirects traffic |
| CLI-082 | Basic-auth in URL leaks | CWE-522 | `user:pass@host` logged/forwarded |
| CLI-083 | Auth re-sent cross-origin | CWE-200 | `Authorization` re-sent after a redirect |
| CLI-084 | Accept-Encoding tampering | CWE-444 | forced encoding triggers a decode bomb |
| CLI-085 | Response charset confusion | CWE-176 | declared charset != body bytes |
| CLI-086 | Cookie store cross-host | CWE-565 | cookies sent to the wrong host |
| CLI-087 | Port 0 / out-of-range port | CWE-20 | `:0` or `:99999` mishandled |
| CLI-088 | IPv6 literal bracket parse | CWE-20 | `[::1]:8080` parsing edge cases |
| CLI-089 | Empty/whitespace URL | CWE-20 | `""` / spaces crash the parser |
| CLI-090 | Extremely long URL | CWE-400 | multi-MB URL exhausts memory |
| CLI-091 | Method case/verb tampering | CWE-20 | lowercase/unknown verbs |
| CLI-092 | TRACE/CONNECT enabled | CWE-16 | dangerous methods reach the wire |
| CLI-093 | Expect/100-continue handling | CWE-400 | continue flow hangs the client |
| CLI-094 | Retry amplification | CWE-400 | automatic retries multiply load |
| CLI-095 | Idempotency on retry | CWE-696 | retried non-idempotent request duplicates side effects |
| CLI-096 | Body streaming backpressure | CWE-400 | large request body buffered fully |
| CLI-097 | Concurrent TLS init race | CWE-362 | first-use SSL manager init races |
| CLI-098 | Error message info leak | CWE-209 | internal error text returned to Lua |
| CLI-099 | Content-Length integer parse | CWE-190 | crafted response Content-Length overflows |
| CLI-100 | Non-UTF8 header values | CWE-176 | raw bytes in response headers |

---

## Additional cases (documented attacks & CVEs)

### SSRF — documented techniques & targets

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-101 | Capital One-style IMDS theft | CWE-918 | SSRF → IMDSv1 creds (2019 breach pattern) |
| CLI-102 | IMDSv2 token bypass attempt | CWE-918 | reaching the PUT token endpoint via SSRF |
| CLI-103 | GCP metadata `Metadata-Flavor` | CWE-918 | header-gated metadata still reachable |
| CLI-104 | Azure IMDS `Metadata: true` | CWE-918 | Azure metadata endpoint |
| CLI-105 | Kubernetes API/service IP | CWE-918 | `10.0.0.1`/`kubernetes.default` |
| CLI-106 | Docker socket via TCP | CWE-918 | `:2375` daemon reachable |
| CLI-107 | Redis/Memcached SSRF | CWE-918 | `gopher://`-style or inline commands |
| CLI-108 | Elasticsearch/Solr SSRF | CWE-918 | internal `:9200`/`:8983` |
| CLI-109 | Internal admin panel reach | CWE-918 | `localhost:` dashboards |
| CLI-110 | Blind SSRF (timing/OOB) | CWE-918 | confirm via timing or DNS callback |
| CLI-111 | SSRF to cloud function/metadata IPv6 | CWE-918 | `[fd00:ec2::254]` |
| CLI-112 | PDF/SSRF via fetched resource | CWE-918 | server fetches an attacker URL |

### SSRF — parser/encoding bypass (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-113 | Orange Tsai URL parser confusion | CWE-918 | "A New Era of SSRF" parser differential |
| CLI-114 | `http://a@b@c/` double-at | CWE-918 | ambiguous userinfo |
| CLI-115 | `http://a#@b/` fragment trick | CWE-918 | fragment hides the real host |
| CLI-116 | Backslash host split | CWE-918 | `http://expected\@evil/` |
| CLI-117 | Whitespace/tab in URL | CWE-918 | embedded control chars |
| CLI-118 | CR/LF in URL host | CWE-113 | host with line breaks |
| CLI-119 | Percent-encoded host | CWE-918 | `127%2e0%2e0%2e1` |
| CLI-120 | Dotless decimal + path | CWE-918 | `http://2130706433:80/x` |
| CLI-121 | IPv6 scoped/embedded IPv4 | CWE-918 | `[::ffff:7f00:1]` |
| CLI-122 | IDNA/punycode host | CWE-918 | unicode host normalizes internally |
| CLI-123 | Enclosing-bracket trick | CWE-918 | malformed `[host]` parsing |
| CLI-124 | Trailing-dot FQDN | CWE-918 | `localhost.` bypasses an allowlist |

### Redirect / DNS (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-125 | 30x to `file://` | CWE-918 | redirect changes scheme to file |
| CLI-126 | 30x to internal after allow | CWE-918 | first hop allowed, redirect internal |
| CLI-127 | Open-redirect chain to metadata | CWE-918 | `r3dir`-style chain to IMDS |
| CLI-128 | DNS rebinding (TTL 0) | CWE-918 | rebind between check and connect |
| CLI-129 | DNS A/AAAA split-horizon | CWE-918 | IPv4 safe, IPv6 internal |
| CLI-130 | Multiple A records | CWE-918 | one safe, one internal |
| CLI-131 | DNS pinning absent across redirects | CWE-918 | re-resolution per hop |
| CLI-132 | CRLF in redirect Location | CWE-113 | header injection on follow |

### TLS / certificate (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-133 | NULL-byte CN | CVE-2009-2408 | `evil.com\0.good.com` cert CN |
| CLI-134 | Wildcard cert over-match | CWE-295 | `*.com`/multi-label wildcard |
| CLI-135 | Hostname-verify disabled by default | CWE-295 | verify mode left relaxed |
| CLI-136 | Missing SAN check (CN-only) | CWE-297 | trusting CN, ignoring SAN |
| CLI-137 | Chain-of-trust not built | CWE-295 | intermediate not verified |
| CLI-138 | Basic-constraints not checked | CWE-295 | a leaf used as a CA |
| CLI-139 | Self-signed accepted (handler) | CWE-295 | accept-all certificate handler |
| CLI-140 | Renegotiation/`CVE-2009-3555` | CWE-300 | client reneg injection |
| CLI-141 | TLS compression (CRIME) | CVE-2012-4929 | compression side-channel |
| CLI-142 | Downgrade to TLS 1.0 | CWE-757 | MITM forces legacy protocol |
| CLI-143 | OCSP/CRL not consulted | CWE-299 | revoked cert accepted |
| CLI-144 | OpenSSL lib CVE surface | CWE-1395 | unpatched client TLS lib |

### Library/CVE-class & response

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-145 | curl-style URL glob/control | CWE-20 | unexpected URL features |
| CLI-146 | Header-folding (obs-fold) | CWE-444 | obsolete line folding in response |
| CLI-147 | Response splitting via redirect | CWE-113 | CRLF in a redirected request |
| CLI-148 | gzip bomb response | CWE-409 | small body, huge inflate |
| CLI-149 | brotli/deflate bomb | CWE-409 | other content-encodings |
| CLI-150 | Nested content-encoding | CWE-409 | `Content-Encoding: gzip, gzip` |
| CLI-151 | Chunk-ext flood in response | CWE-400 | huge chunk extensions |
| CLI-152 | Trailer injection in response | CWE-444 | response trailers add headers |
| CLI-153 | Content-Length vs body mismatch | CWE-444 | client trusts CL over actual |
| CLI-154 | Status-line smuggling | CWE-444 | malformed status line |
| CLI-155 | Set-Cookie injection on redirect | CWE-565 | response sets cookies cross-host |
| CLI-156 | Compression-ratio guard absent | CWE-409 | no decoded-size cap |

### Request injection & header (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-157 | CRLF header injection (curl-class) | CWE-93 | newline in a custom header |
| CLI-158 | Host header override to bypass | CWE-444 | client-set Host differs from URL |
| CLI-159 | Smuggling via client CL/TE | CWE-444 | conflicting framing the client adds |
| CLI-160 | Authorization leak on cross-host redirect | CWE-200 | credentials re-sent (curl CVE-class) |
| CLI-161 | Cookie leak on cross-host redirect | CWE-200 | cookies re-sent to a new host |
| CLI-162 | Proxy header injection | CWE-441 | `Proxy-*` headers honored |
| CLI-163 | `Expect: 100-continue` mishandle | CWE-444 | body/continue desync |
| CLI-164 | TE in outbound request | CWE-444 | client emits chunked unexpectedly |
| CLI-165 | Method spoofing (CONNECT) | CWE-16 | `CONNECT` tunneling |

### State, concurrency, resource (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-166 | Connection-pool identity confusion | CWE-441 | response for the wrong request |
| CLI-167 | TLS-session-resumption cross-host | CWE-441 | resumed session reused wrongly |
| CLI-168 | 0-RTT early-data replay | CWE-294 | TLS 1.3 early data replayed |
| CLI-169 | Keep-alive race on reuse | CWE-362 | reused socket mid-response |
| CLI-170 | Unbounded redirect loop | CWE-835 | A→B→A redirect cycle |
| CLI-171 | Retry storm amplification | CWE-405 | retries multiply against a target |
| CLI-172 | DNS resolution DoS | CWE-400 | slow/large DNS responses |
| CLI-173 | Connect to closed port flood | CWE-400 | port-scan-as-DoS |
| CLI-174 | Memory per concurrent request | CWE-400 | many in-flight responses buffered |
| CLI-175 | Slow-DNS / slow-connect hang | CWE-400 | no connect timeout |
| CLI-176 | First-use TLS init race | CWE-362 | SSL manager initialized concurrently |
| CLI-177 | Socket fd leak on error | CWE-404 | session not closed on failure |

### Misc / protocol / fuzz (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-178 | HTTP/0.9 response handling | CWE-444 | bodyless legacy response |
| CLI-179 | Mixed HTTP version response | CWE-436 | version downgrade in reply |
| CLI-180 | UTF-7/charset response confusion | CWE-176 | charset-driven misinterpretation |
| CLI-181 | BOM in response body | CWE-176 | leading BOM mishandled |
| CLI-182 | Huge header value | CWE-400 | multi-MB single header |
| CLI-183 | Duplicate Content-Length (response) | CWE-444 | two CL values |
| CLI-184 | Negative/overflow CL (response) | CWE-190 | CL parse overflow |
| CLI-185 | URL with embedded credentials logged | CWE-532 | `user:pass@` written to logs |
| CLI-186 | Error message reveals target IP | CWE-209 | internal IP in an error |
| CLI-187 | Timing reveals internal reachability | CWE-204 | blind-SSRF confirmation |
| CLI-188 | Response-size oracle | CWE-204 | length differences leak state |
| CLI-189 | Differential dummy/native client | CWE-697 | wasm-fetch vs native checks diverge |
| CLI-190 | Content-Length vs chunked spoof | CWE-444 | response forges Content-Length against chunked framing |
| CLI-191 | NUL in URL | CWE-626 | URL truncation at NUL |
| CLI-192 | Extremely long URL | CWE-400 | multi-MB URL |
| CLI-193 | Unicode normalization of URL | CWE-178 | host normalized after a check |
| CLI-194 | Port 0 / overflow port | CWE-190 | invalid port handling |
| CLI-195 | IPv6 zone-id injection | CWE-918 | `%25` zone id in host |
| CLI-196 | Scheme-relative `//host` | CWE-918 | protocol inherited unexpectedly |
| CLI-197 | data:/blob: scheme | CWE-918 | non-network scheme handling |
| CLI-198 | TRACE method reflection | CWE-16 | XST-style reflection |
| CLI-199 | Idempotency on auto-retry | CWE-696 | retried POST duplicates effects |
| CLI-200 | Fuzz URL+headers+body | CWE-20 | coverage-guided fuzz never crashes |

---

## Round 3 — deeper / documented

### SSRF gadgets & targets (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-201 | gopher:// to SMTP/Redis | CWE-918 | crafted multi-line payload to a TCP service |
| CLI-202 | dict:// service probe | CWE-918 | banner-grab internal services |
| CLI-203 | ftp:// active-mode pivot | CWE-918 | FTP command channel abuse |
| CLI-204 | tftp:// internal read | CWE-918 | UDP file fetch |
| CLI-205 | sftp/ssh probe | CWE-918 | internal port reachability |
| CLI-206 | Consul/etcd API reach | CWE-918 | `:8500`/`:2379` config stores |
| CLI-207 | Vault API reach | CWE-918 | `:8200` secret store |
| CLI-208 | Prometheus/Grafana reach | CWE-918 | internal dashboards |
| CLI-209 | Internal Git/CI reach | CWE-918 | `:3000`/`:8080` services |
| CLI-210 | Cloud SQL proxy reach | CWE-918 | local DB proxy |
| CLI-211 | Link-local IPv6 metadata | CWE-918 | `[fe80::]`/`[fd00:ec2::254]` |
| CLI-212 | Carrier-grade NAT range | CWE-918 | `100.64.0.0/10` |
| CLI-213 | Broadcast/multicast target | CWE-406 | sending to a group address |

### SSRF parser bypass (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-214 | Mixed octal/decimal/hex octets | CWE-918 | `0x7f.0.0.1` style |
| CLI-215 | Dword + path | CWE-918 | `http://2130706433/admin` |
| CLI-216 | Zero-padded octets | CWE-918 | `127.000.000.001` |
| CLI-217 | Fullwidth/IDN digits | CWE-918 | unicode-confusable host |
| CLI-218 | Tab/newline in URL | CWE-113 | control bytes split parsing |
| CLI-219 | Backslash path/host split | CWE-918 | `http://good\@evil` |
| CLI-220 | Fragment-hidden host | CWE-918 | `http://evil#@good` |
| CLI-221 | Userinfo with percent-encoding | CWE-918 | `%40` decodes to `@` |
| CLI-222 | Trailing-dot allowlist bypass | CWE-918 | `internal.host.` |
| CLI-223 | Case-only allowlist bypass | CWE-178 | host case differs from the rule |
| CLI-224 | Validator vs client URL parse | CWE-436 | the two disagree on the host |
| CLI-225 | Double-encoded host | CWE-918 | `%2569` style |

### Redirect / DNS (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-226 | Redirect scheme change to gopher | CWE-918 | 30x switches protocol |
| CLI-227 | Redirect host re-validation absent | CWE-918 | only the first hop checked |
| CLI-228 | Cross-protocol redirect (https→http) | CWE-319 | downgrade on follow |
| CLI-229 | Meta-refresh/JS not relevant (server fetch) | CWE-918 | only HTTP 30x relevant |
| CLI-230 | DNS TTL-0 rebinding | CWE-918 | re-resolve to internal |
| CLI-231 | DNS multi-record selection | CWE-918 | pick an internal A/AAAA |
| CLI-232 | DNS pinning across redirects | CWE-918 | re-resolution per hop |
| CLI-233 | Happy-eyeballs v4/v6 race | CWE-918 | v6 internal wins |

### TLS / certificate (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-234 | SAN wildcard over-match | CWE-295 | `*.example` matches too much |
| CLI-235 | IP-SAN vs DNS-SAN confusion | CWE-297 | connecting by IP |
| CLI-236 | Internationalized-name match | CWE-297 | IDN cert name handling |
| CLI-237 | Trust-store override via env | CWE-295 | `SSL_CERT_FILE` hijack |
| CLI-238 | Client-cert leakage | CWE-295 | wrong client cert presented |
| CLI-239 | Session-ticket reuse cross-host | CWE-441 | resumed to the wrong origin |
| CLI-240 | Renegotiation injection | CVE-2009-3555 | client reneg |
| CLI-241 | ALPN mismatch | CWE-436 | negotiated protocol wrong |
| CLI-242 | TLS 1.3 0-RTT replay | CWE-294 | early-data on a retryable request |
| CLI-243 | Min-protocol not enforced | CWE-326 | legacy TLS accepted |

### Response / encoding / DoS (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-244 | Content-Encoding chain bomb | CWE-409 | `gzip, gzip, br` stacking |
| CLI-245 | Transfer + Content-Encoding combo | CWE-444 | TE/CE interaction |
| CLI-246 | Decoded-size cap absent | CWE-409 | cap applies to compressed only |
| CLI-247 | Obs-fold header in response | CWE-444 | line folding revival |
| CLI-248 | Status-line CRLF injection | CWE-113 | malformed status reflected |
| CLI-249 | Header value with NUL | CWE-626 | NUL in a response header |
| CLI-250 | Charset-driven misinterpret | CWE-176 | UTF-7/UTF-16 response |
| CLI-251 | BOM-led body | CWE-176 | leading BOM |
| CLI-252 | Trailer header override | CWE-444 | trailers add/override headers |
| CLI-253 | Pipelined response misalignment | CWE-444 | response queue poisoning |
| CLI-254 | Slow-trickle response | CWE-400 | drip read holds the worker |
| CLI-255 | DNS-response DoS | CWE-400 | huge/slow DNS reply |

### Request injection & headers (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-256 | Header name with CR | CWE-113 | bare CR in a name |
| CLI-257 | Header value with LF | CWE-113 | bare LF in a value |
| CLI-258 | Duplicate Host in request | CWE-444 | two Host headers |
| CLI-259 | Authorization on redirect host | CWE-200 | credentials re-sent (curl CVE class) |
| CLI-260 | Cookie sent to redirect host | CWE-200 | cookie leaks cross-host |
| CLI-261 | Proxy-Authorization leak | CWE-522 | proxy creds forwarded |
| CLI-262 | Method override smuggling | CWE-444 | conflicting method/framing |
| CLI-263 | Expect/100-continue desync | CWE-444 | body sent before continue |

### Connection / state / concurrency (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-264 | Pool identity confusion | CWE-441 | reused connection crosses identities |
| CLI-265 | Keep-alive response misroute | CWE-664 | response for a different request |
| CLI-266 | Connection reuse after error | CWE-664 | tainted socket reused |
| CLI-267 | Redirect loop (no cap) | CWE-835 | A→B→A cycle |
| CLI-268 | Retry storm against a target | CWE-405 | retries amplify load |
| CLI-269 | Concurrent-request fd leak | CWE-404 | sessions not closed |
| CLI-270 | TLS init race (first use) | CWE-362 | SSL manager concurrency |
| CLI-271 | Reentrant request in callback | CWE-674 | request from a resolve callback |
| CLI-272 | Cross-thread settle race | CWE-362 | worker settles to the loop |
| CLI-273 | Promise leak on error | CWE-401 | refs not released |

### Misc / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CLI-274 | HTTP/0.9 response | CWE-444 | bodyless legacy reply |
| CLI-275 | HTTP version downgrade reply | CWE-436 | server claims a lower version |
| CLI-276 | data:/blob:/javascript: scheme | CWE-918 | non-network scheme |
| CLI-277 | URL with embedded NUL | CWE-626 | truncation at NUL |
| CLI-278 | URL fragment handling | CWE-20 | fragment sent to the server |
| CLI-279 | Percent-encoding in path | CWE-113 | encoded CRLF in the path |
| CLI-280 | Port out of range | CWE-190 | `:99999` |
| CLI-281 | IPv6 literal bracket fuzz | CWE-20 | malformed `[..]` host |
| CLI-282 | Very long header value | CWE-400 | multi-MB header |
| CLI-283 | Many request headers | CWE-400 | header flood |
| CLI-284 | Timeout integer overflow | CWE-190 | `timeoutSeconds` near limits |
| CLI-285 | maxResponseBytes overflow | CWE-190 | huge cap wraps the size |
| CLI-286 | Negative/zero timeout | CWE-20 | non-positive timeout |
| CLI-287 | Body NUL safety | CWE-626 | request body with NUL |
| CLI-288 | Response NUL safety | CWE-626 | NUL in the returned body |
| CLI-289 | Header map non-string keys | CWE-20 | non-string Lua header keys |
| CLI-290 | Content-Length parse overflow | CWE-190 | crafted response Content-Length integer |
| CLI-291 | Error reveals resolved IP | CWE-209 | internal IP in an error |
| CLI-292 | Timing confirms reachability | CWE-204 | blind-SSRF oracle |
| CLI-293 | Response-size oracle | CWE-204 | length leaks state |
| CLI-294 | Basic-auth in URL logged | CWE-532 | credentials in logs |
| CLI-295 | Proxy from environment | CWE-441 | `*_proxy` redirects traffic |
| CLI-296 | TRACE/XST reflection | CWE-16 | method reflection |
| CLI-297 | Idempotency on POST retry | CWE-696 | duplicated side effects |
| CLI-298 | wasm-fetch vs native parity | CWE-697 | browser path skips checks |
| CLI-299 | ASan/UBSan trip on response parse | CWE-125 | malformed response |
| CLI-300 | Differential vs reference client | CWE-697 | behavior diverges from curl/libcurl |
