# 🔌 socket

Covers `socket.tcp.connect/listen/accept/send/receive/close`, `socket.udp.bind/sendTo/recvFrom/close`. All async (promises).

### Address & connection (SSRF-class)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-001 | Connect to loopback | CWE-918 | Reach a local-only service |
| SOCK-002 | Connect to internal range | CWE-918 | RFC1918 / link-local targets |
| SOCK-003 | Connect to metadata IP | CWE-918 | The metadata address `169.254.169.254` |
| SOCK-004 | Connect to internal admin port | CWE-918 | Ports 6379/9200/5432 from user input |
| SOCK-005 | IPv6 literal handling | CWE-20 | Edge cases parsing `[::1]` |
| SOCK-006 | IPv4-mapped IPv6 | CWE-918 | The mapped address `[::ffff:127.0.0.1]` |
| SOCK-007 | Hostname resolution SSRF | CWE-918 | Name resolves to an internal IP |
| SOCK-008 | DNS rebinding on reconnect | CWE-918 | Resolution changes between connects |
| SOCK-009 | Port 0 / out-of-range | CWE-20 | Handling of `:0`, `:99999` |
| SOCK-010 | Negative port | CWE-20 | Negative port value |
| SOCK-011 | Empty/garbage host | CWE-20 | An `""`/junk host |
| SOCK-012 | Host with control chars | CWE-74 | Control bytes in host |
| SOCK-013 | Very long host string | CWE-400 | Multi-MB host |
| SOCK-014 | Broadcast/multicast address | CWE-406 | Sending to a broadcast address |
| SOCK-015 | Bind to privileged port | CWE-269 | Binding `:80`/`:443` without privilege handling |
| SOCK-016 | Bind to all interfaces | CWE-668 | The address `0.0.0.0` unexpectedly public |
| SOCK-017 | Connect timeout absent | CWE-400 | Connect to a blackhole hangs |
| SOCK-018 | Reverse-DNS trust | CWE-350 | Trusting PTR records for authz |

### Receive / send buffer handling

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-019 | Huge `maxBytes` | CWE-789 | Giant receive size over-allocates |
| SOCK-020 | Negative `maxBytes` | CWE-20 | Negative size cast |
| SOCK-021 | Zero `maxBytes` | CWE-20 | Zero-length receive semantics |
| SOCK-022 | `maxBytes` integer overflow | CWE-190 | Size near `INT_MAX` |
| SOCK-023 | Receive buffer overflow | CWE-120 | C buffer smaller than received data |
| SOCK-024 | Receive OOB write | CWE-787 | Length mishandled on read |
| SOCK-025 | Partial read handling | CWE-20 | Short read returns correctly |
| SOCK-026 | NUL in received data | CWE-626 | Binary data not truncated |
| SOCK-027 | NUL in sent data | CWE-626 | The `send` call is length-aware |
| SOCK-028 | Send of huge buffer | CWE-400 | Multi-GB send buffered |
| SOCK-029 | Non-string send data | CWE-20 | Number/table coercion |
| SOCK-030 | Uninitialized buffer read | CWE-457 | Unread bytes returned |
| SOCK-031 | Buffer reuse across reads | CWE-664 | Stale bytes from a prior read |
| SOCK-032 | `recvFrom` truncation | CWE-130 | Datagram larger than buffer truncated silently |
| SOCK-033 | `recvFrom` sender spoofing | CWE-290 | Trusting the reported source address |
| SOCK-034 | Send to unbound UDP | CWE-20 | Calling `sendTo` before `bind` |

### Resource / DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-035 | FD exhaustion (`connect`) | CWE-404 | Many connections never closed |
| SOCK-036 | FD exhaustion (`accept`) | CWE-404 | Accepted sockets leak |
| SOCK-037 | Listener leak | CWE-404 | Listeners not closed |
| SOCK-038 | Accept flood | CWE-400 | Connection storm exhausts the pool |
| SOCK-039 | Slowloris on a TCP service | CWE-400 | Drip-fed bytes hold a handler |
| SOCK-040 | Half-open connections | CWE-400 | SYN/half-open accumulation |
| SOCK-041 | UDP flood | CWE-400 | Packet storm |
| SOCK-042 | UDP amplification | CWE-406 | Small request → large reply to a spoofed source |
| SOCK-043 | Memory per connection | CWE-400 | Per-conn buffers exhaust memory |
| SOCK-044 | Backlog overflow | CWE-400 | Listen backlog exceeded |
| SOCK-045 | Idle connection hoarding | CWE-400 | Many idle sockets held open |
| SOCK-046 | Send buffer bloat | CWE-400 | Unbounded queued send data |

### Blocking & concurrency

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-047 | Blocking receive on main loop | CWE-400 | Blocking call stalls the loop |
| SOCK-048 | Blocking accept on main loop | CWE-400 | The `accept` call blocks the loop |
| SOCK-049 | Blocking connect | CWE-400 | The `connect` call blocks without timeout |
| SOCK-050 | Cross-thread settle race | CWE-362 | Worker settles a socket promise to the loop |
| SOCK-051 | Concurrent ops on one socket | CWE-362 | Parallel send/receive race |
| SOCK-052 | Close during pending op | CWE-416 | Calling `close` while a read is in flight |
| SOCK-053 | Double close | CWE-675 | Calling `close` twice |
| SOCK-054 | Use after close | CWE-416 | Calling `send`/`receive` on a closed socket |
| SOCK-055 | Listener accept after close | CWE-416 | Calling `accept` on a closed listener |
| SOCK-056 | Promise ref leak | CWE-401 | Op promise refs not released |
| SOCK-057 | GC of in-use socket | CWE-416 | Socket collected during an op |
| SOCK-058 | Reentrant op in callback | CWE-674 | Issuing ops from a resolve callback |
| SOCK-059 | Shutdown race | CWE-362 | Runtime stop while sockets are active |
| SOCK-060 | Task-pool starvation | CWE-400 | Blocking socket ops exhaust the pool |

### Protocol / data handling

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-061 | TCP message framing | CWE-20 | No length framing → boundary confusion |
| SOCK-062 | Injection in app protocol | CWE-74 | CRLF/control bytes in a text protocol |
| SOCK-063 | Line-based protocol overflow | CWE-120 | Line without terminator grows unbounded |
| SOCK-064 | Binary protocol length field | CWE-130 | Trusting an attacker length prefix |
| SOCK-065 | Endianness in length field | CWE-188 | Byte order mismatch |
| SOCK-066 | UDP packet size limit | CWE-20 | Oversized datagram handling |
| SOCK-067 | Fragmented datagram | CWE-20 | IP fragmentation edge cases |
| SOCK-068 | Connection reset handling | CWE-703 | RST mid-operation |
| SOCK-069 | EOF/zero-length read | CWE-20 | Peer close detected correctly |
| SOCK-070 | Keep-alive/timeout config | CWE-400 | No read/idle timeout |

### Errors, info, fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-071 | Connect-refused error | CWE-20 | Clean reject, no crash |
| SOCK-072 | Address-in-use error | CWE-20 | The `EADDRINUSE` error surfaced |
| SOCK-073 | Permission-denied bind | CWE-20 | The `EACCES` error handled |
| SOCK-074 | Error info leak | CWE-209 | Error text reveals no internals |
| SOCK-075 | Exception across boundary | CWE-248 | Socket exceptions caught |
| SOCK-076 | Stack imbalance on error | CWE-664 | Error path leaves a balanced Lua stack |
| SOCK-077 | Host fuzz | CWE-20 | Random hosts never crash |
| SOCK-078 | Port fuzz | CWE-20 | Random ports handled |
| SOCK-079 | Payload fuzz (`send`) | CWE-20 | Random/binary payloads |
| SOCK-080 | Received-data fuzz (handler) | CWE-20 | Random bytes to a receive handler |
| SOCK-081 | `maxBytes` fuzz | CWE-20 | Random sizes incl. extremes |
| SOCK-082 | Rapid open/close churn | CWE-404 | Churn does not leak fds/memory |
| SOCK-083 | TLS-over-socket (if added) | CWE-295 | Cert verification for secure sockets |
| SOCK-084 | `SO_REUSEADDR/PORT` hijack | CWE-668 | Another process binds the same port |
| SOCK-085 | Bind address spoof trust | CWE-290 | Trusting local-bind as authentication |
| SOCK-086 | Connection-count limit absent | CWE-770 | No per-client connection cap |
| SOCK-087 | Read-size amplification | CWE-400 | Tiny request triggers a huge read loop |
| SOCK-088 | Nagle/latency assumptions | CWE-20 | Message boundaries assumed by timing |
| SOCK-089 | Signal interruption (`EINTR`) | CWE-703 | Interrupted syscall handling |
| SOCK-090 | Non-blocking `errno` (`EAGAIN`) | CWE-703 | Would-block handled |
| SOCK-091 | Half-close (`shutdown`) | CWE-20 | One-way shutdown semantics |
| SOCK-092 | Out-of-band/urgent data | CWE-20 | OOB byte handling |
| SOCK-093 | Multicast group join abuse | CWE-668 | Joining arbitrary groups |
| SOCK-094 | IP options/TTL handling | CWE-20 | Crafted IP options |
| SOCK-095 | Source-port predictability | CWE-330 | Predictable ephemeral ports |
| SOCK-096 | Concurrent bind same addr | CWE-362 | Race binding the same address |
| SOCK-097 | `recvFrom` address parse | CWE-20 | Malformed sender address struct |
| SOCK-098 | Huge UDP `sendTo` | CWE-400 | Oversized datagram send |
| SOCK-099 | Integer overflow in size math | CWE-190 | Length arithmetic on huge buffers |
| SOCK-100 | Fuzz all entry points | CWE-20 | Combined random inputs never crash |

---

## Additional cases (documented attacks & CVEs)

### Network DoS (named/documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-101 | SYN flood | CWE-941 | Half-open connection storm |
| SOCK-102 | ACK/RST flood | CWE-400 | Stateless flood exhausts CPU |
| SOCK-103 | Slowloris (server socket) | CWE-400 | Drip-fed bytes hold handlers |
| SOCK-104 | Slow read window | CWE-400 | Tiny window stalls sends |
| SOCK-105 | Connection-table exhaustion | CWE-400 | Many established sockets |
| SOCK-106 | Ephemeral-port exhaustion | CWE-400 | Client-side port depletion |
| SOCK-107 | UDP flood | CWE-400 | Packet storm |
| SOCK-108 | UDP amplification (DNS/NTP/memcached) | CWE-406 | Small request → large reply to a spoofed source |
| SOCK-109 | Reflection to a victim | CWE-406 | Spoofed source address reflection |
| SOCK-110 | Fragmentation flood | CWE-400 | Many IP fragments to reassemble |
| SOCK-111 | Sockstress-style | CWE-400 | Zero-window resource hold |
| SOCK-112 | Accept-storm pool exhaustion | CWE-400 | Accepting faster than handlers free |
| SOCK-113 | Per-client connection cap absent | CWE-770 | One client opens thousands |
| SOCK-114 | Buffer-bloat send queue | CWE-400 | Unbounded queued send data |

### Spoofing / hijacking (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-115 | TCP sequence prediction | CWE-330 | Predictable ISN hijack |
| SOCK-116 | Blind TCP reset injection | CWE-300 | Off-path RST |
| SOCK-117 | UDP source spoofing | CWE-290 | Trusting the reported sender |
| SOCK-118 | `recvFrom` address trust | CWE-290 | Authz based on source IP |
| SOCK-119 | ARP/neighbor spoof assumption | CWE-300 | Trusting L2 identity |
| SOCK-120 | Reverse-DNS trust | CWE-350 | Authz from a PTR record |
| SOCK-121 | Source-port predictability | CWE-330 | Guessable ephemeral ports |
| SOCK-122 | Connection hijack on reuse | CWE-441 | Reused socket crosses identities |
| SOCK-123 | Man-in-the-middle (no TLS) | CWE-319 | Plaintext socket interception |

### SSRF / egress (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-124 | Connect to loopback service | CWE-918 | Local-only daemon reached |
| SOCK-125 | Connect to metadata IP | CWE-918 | The metadata address `169.254.169.254` |
| SOCK-126 | Connect to internal range | CWE-918 | RFC1918 targets |
| SOCK-127 | Internal admin ports | CWE-918 | Ports 6379/9200/2375 etc. |
| SOCK-128 | DNS-resolved internal host | CWE-918 | Name resolves internal |
| SOCK-129 | DNS rebinding on reconnect | CWE-918 | Resolution changes per connect |
| SOCK-130 | IPv4-mapped IPv6 bypass | CWE-918 | The mapped address `[::ffff:127.0.0.1]` |
| SOCK-131 | Decimal/octal/hex host | CWE-918 | Encoded IP forms |
| SOCK-132 | Bind exposes service publicly | CWE-668 | Unintended exposure via `0.0.0.0` |
| SOCK-133 | Egress to arbitrary port | CWE-918 | Data exfiltration channel |
| SOCK-134 | gopher-style raw payload | CWE-918 | Crafted bytes to a TCP service |

### Buffer / data handling (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-135 | Receive buffer overflow | CWE-120 | C buffer < received length |
| SOCK-136 | Receive OOB write | CWE-787 | Length mishandled |
| SOCK-137 | Negative/huge `maxBytes` | CWE-190 | Size cast overflow |
| SOCK-138 | Zero `maxBytes` semantics | CWE-20 | Empty-read behavior |
| SOCK-139 | Datagram truncation | CWE-130 | Datagram > buffer truncated silently |
| SOCK-140 | NUL in received data | CWE-626 | Binary not truncated |
| SOCK-141 | NUL in send data | CWE-626 | The `send` call is length-aware |
| SOCK-142 | Uninitialized buffer return | CWE-457 | Stale bytes returned |
| SOCK-143 | Buffer reuse across reads | CWE-664 | Prior-read bytes leak |
| SOCK-144 | Partial send not retried | CWE-393 | Short write loses data |
| SOCK-145 | Message framing confusion | CWE-130 | No length framing on a stream |
| SOCK-146 | Length-prefix trust | CWE-130 | Attacker length prefix |
| SOCK-147 | Endianness of length field | CWE-188 | Byte-order mismatch |
| SOCK-148 | Line-based overflow | CWE-120 | Unterminated line grows unbounded |

### Lifecycle / concurrency (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-149 | Use after close | CWE-416 | Op on a closed socket |
| SOCK-150 | Double close | CWE-675 | Calling `close` twice |
| SOCK-151 | Close during pending read | CWE-416 | Calling `close` while a read is in flight |
| SOCK-152 | GC of in-use socket | CWE-416 | Socket collected mid-op |
| SOCK-153 | FD leak (`connect`) | CWE-404 | Connection never closed |
| SOCK-154 | FD leak (`accept`) | CWE-404 | Accepted socket leaks |
| SOCK-155 | Listener leak | CWE-404 | Listener not closed |
| SOCK-156 | Concurrent ops on one socket | CWE-362 | Parallel `send`/`recv` race |
| SOCK-157 | Cross-thread settle race | CWE-362 | Worker settles to the loop |
| SOCK-158 | Blocking op on main loop | CWE-400 | Blocking `recv`/`accept`/`connect` |
| SOCK-159 | Promise ref leak | CWE-401 | Op refs not released |
| SOCK-160 | Reentrant op in callback | CWE-674 | Op from a resolve callback |
| SOCK-161 | Shutdown race | CWE-362 | Runtime stop with active sockets |
| SOCK-162 | Task-pool starvation | CWE-400 | Blocking ops exhaust the pool |
| SOCK-163 | `EINTR` handling | CWE-703 | Interrupted syscall |
| SOCK-164 | `EAGAIN`/would-block handling | CWE-703 | Non-blocking `errno` |
| SOCK-165 | `EPIPE`/`SIGPIPE` on send | CWE-703 | Write to a closed peer |

### Protocol / platform (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-166 | `SO_REUSEADDR/PORT` hijack | CWE-668 | Another process steals the port |
| SOCK-167 | Privileged-port bind | CWE-269 | Binding < 1024 |
| SOCK-168 | Dual-stack v4/v6 binding | CWE-668 | Unexpected v4-mapped exposure |
| SOCK-169 | `IP_TRANSPARENT`/spoof options | CWE-290 | Socket options enabling spoof |
| SOCK-170 | TTL/IP-options abuse | CWE-20 | Crafted IP options |
| SOCK-171 | OOB/urgent data | CWE-20 | Urgent-pointer handling |
| SOCK-172 | Half-close (`shutdown`) semantics | CWE-20 | One-way shutdown |
| SOCK-173 | Nagle/latency assumption | CWE-20 | Boundaries assumed by timing |
| SOCK-174 | Multicast group join abuse | CWE-668 | Joining arbitrary groups |
| SOCK-175 | Broadcast send | CWE-406 | Broadcast amplification |
| SOCK-176 | Keepalive/idle timeout absent | CWE-400 | Idle sockets held forever |
| SOCK-177 | Connect timeout absent | CWE-400 | Connect to a blackhole hangs |
| SOCK-178 | Linger/abort behavior | CWE-404 | Resource hold under `SO_LINGER` |
| SOCK-179 | Path-MTU/fragmentation edge | CWE-20 | PMTU blackhole |
| SOCK-180 | Port 0 / out-of-range | CWE-20 | Invalid port handling |

### Errors / fuzz / misc

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-181 | Connect-refused error | CWE-20 | Clean reject |
| SOCK-182 | Address-in-use error | CWE-20 | The `EADDRINUSE` error surfaced |
| SOCK-183 | Permission-denied bind | CWE-20 | The `EACCES` error handled |
| SOCK-184 | Error info leak | CWE-209 | Error reveals internals |
| SOCK-185 | Exception across boundary | CWE-248 | Socket exceptions caught |
| SOCK-186 | Stack imbalance on error | CWE-664 | Error path balanced |
| SOCK-187 | Host fuzz | CWE-20 | Random hosts never crash |
| SOCK-188 | Port fuzz | CWE-20 | Random ports handled |
| SOCK-189 | Payload fuzz (`send`) | CWE-20 | Random/binary payloads |
| SOCK-190 | Received-data fuzz (handler) | CWE-20 | Random bytes to a handler |
| SOCK-191 | `maxBytes` fuzz | CWE-20 | Random sizes incl. extremes |
| SOCK-192 | Rapid open/close churn | CWE-404 | No fd/memory leak under churn |
| SOCK-193 | Integer overflow in size math | CWE-190 | Buffer-size arithmetic |
| SOCK-194 | `recvFrom` address-struct parse | CWE-20 | Malformed sender struct |
| SOCK-195 | Huge UDP `sendTo` | CWE-400 | Oversized datagram |
| SOCK-196 | Concurrent bind same addr | CWE-362 | Race binding the same address |
| SOCK-197 | TLS-over-socket (if added) | CWE-295 | Cert verification for secure sockets |
| SOCK-198 | ASan trip on buffer handling | CWE-125 | OOB in `recv`/`send` buffers |
| SOCK-199 | TSan race on shared socket | CWE-362 | Data race flagged |
| SOCK-200 | Fuzz all entry points | CWE-20 | Combined random inputs never crash |

---

## Round 3 — deeper / documented

### Network DoS (deeper / named)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-201 | SYN-cookie bypass | CWE-941 | Flood despite cookies |
| SOCK-202 | LAND attack | CWE-941 | Spoofed same src/dst |
| SOCK-203 | Smurf/Fraggle | CWE-406 | Broadcast amplification |
| SOCK-204 | NTP `monlist` amplification | CWE-406 | Large reply to a spoofed source |
| SOCK-205 | DNS ANY amplification | CWE-406 | Small query, huge reply |
| SOCK-206 | memcached UDP amplification | CWE-406 | Massive reflection |
| SOCK-207 | Sockstress zero-window | CWE-400 | Resource hold |
| SOCK-208 | TCP persist-timer abuse | CWE-400 | Zero-window persist |
| SOCK-209 | Slow-read throttling | CWE-400 | Tiny receive window |
| SOCK-210 | Connection-table fill | CWE-400 | Many established sockets |
| SOCK-211 | Ephemeral-port depletion | CWE-400 | Client port exhaustion |
| SOCK-212 | Per-client cap absent | CWE-770 | One client opens thousands |
| SOCK-213 | Half-open accumulation | CWE-400 | Incomplete handshakes |
| SOCK-214 | Accept-then-idle hoarding | CWE-400 | Accepted but never serviced |

### Spoofing / hijacking (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-215 | TCP ISN prediction | CWE-330 | Guessable sequence numbers |
| SOCK-216 | Blind RST injection | CWE-300 | Off-path reset |
| SOCK-217 | Blind data injection | CWE-300 | Off-path segment |
| SOCK-218 | UDP source spoofing | CWE-290 | Forged sender address |
| SOCK-219 | `recvFrom` address trust | CWE-290 | Authz on source IP |
| SOCK-220 | Reverse-DNS trust | CWE-350 | Authz from a PTR record |
| SOCK-221 | Source-port predictability | CWE-330 | Guessable ephemeral port |
| SOCK-222 | Connection reuse hijack | CWE-441 | Reused socket crosses identities |
| SOCK-223 | MITM without TLS | CWE-319 | Plaintext interception |
| SOCK-224 | ARP/neighbor-spoof assumption | CWE-300 | Trusting L2 identity |

### SSRF / egress (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-225 | Connect to loopback service | CWE-918 | Local-only daemon |
| SOCK-226 | Connect to metadata IP | CWE-918 | The metadata address `169.254.169.254` |
| SOCK-227 | Connect to RFC1918 range | CWE-918 | Internal targets |
| SOCK-228 | Internal admin ports | CWE-918 | Ports 6379/9200/2375 |
| SOCK-229 | DNS-resolved internal host | CWE-918 | Name resolves internal |
| SOCK-230 | DNS rebinding on reconnect | CWE-918 | Resolution changes |
| SOCK-231 | IPv4-mapped IPv6 bypass | CWE-918 | The mapped address `[::ffff:127.0.0.1]` |
| SOCK-232 | Encoded IP host | CWE-918 | Decimal/octal/hex |
| SOCK-233 | Bind exposes service publicly | CWE-668 | An unintended `0.0.0.0` bind |
| SOCK-234 | Egress to arbitrary port | CWE-918 | Exfiltration channel |
| SOCK-235 | gopher-style raw payload | CWE-918 | Crafted bytes to a service |

### Buffer / data handling (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-236 | Receive buffer overflow | CWE-120 | C buffer < received length |
| SOCK-237 | Receive OOB write | CWE-787 | Length mishandled |
| SOCK-238 | Negative/huge `maxBytes` | CWE-190 | Size cast overflow |
| SOCK-239 | Zero `maxBytes` semantics | CWE-20 | Empty-read behavior |
| SOCK-240 | Datagram truncation | CWE-130 | Datagram > buffer truncated |
| SOCK-241 | NUL in received data | CWE-626 | Binary not truncated |
| SOCK-242 | NUL in send data | CWE-626 | The `send` call is length-aware |
| SOCK-243 | Uninitialized buffer return | CWE-457 | Stale bytes returned |
| SOCK-244 | Buffer reuse across reads | CWE-664 | Prior-read bytes leak |
| SOCK-245 | Partial send not retried | CWE-393 | Short write loses data |
| SOCK-246 | Length-prefix trust | CWE-130 | Attacker length prefix |
| SOCK-247 | Endianness of length field | CWE-188 | Byte-order mismatch |
| SOCK-248 | Line-based overflow | CWE-120 | Unterminated line grows |
| SOCK-249 | Framing confusion (no length) | CWE-130 | Boundary confusion on a stream |

### Lifecycle / concurrency (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-250 | Use after close | CWE-416 | Op on a closed socket |
| SOCK-251 | Double close | CWE-675 | Calling `close` twice |
| SOCK-252 | Close during pending read | CWE-416 | Calling `close` mid-read |
| SOCK-253 | GC of in-use socket | CWE-416 | Collected mid-op |
| SOCK-254 | FD leak (`connect`) | CWE-404 | Never closed |
| SOCK-255 | FD leak (`accept`) | CWE-404 | Accepted socket leaks |
| SOCK-256 | Listener leak | CWE-404 | Listener not closed |
| SOCK-257 | Concurrent ops on one socket | CWE-362 | Parallel `send`/`recv` race |
| SOCK-258 | Cross-thread settle race | CWE-362 | Worker settles to the loop |
| SOCK-259 | Blocking op on main loop | CWE-400 | Blocking `recv`/`accept`/`connect` |
| SOCK-260 | Promise ref leak | CWE-401 | Refs not released |
| SOCK-261 | Reentrant op in callback | CWE-674 | Op from a resolve callback |
| SOCK-262 | Shutdown race | CWE-362 | Stop with active sockets |
| SOCK-263 | Task-pool starvation | CWE-400 | Blocking ops exhaust the pool |
| SOCK-264 | `EINTR` handling | CWE-703 | Interrupted syscall |
| SOCK-265 | `EAGAIN`/would-block | CWE-703 | Non-blocking `errno` |
| SOCK-266 | `EPIPE`/`SIGPIPE` on send | CWE-703 | Write to a closed peer |

### Protocol / platform / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| SOCK-267 | `SO_REUSEADDR/PORT` hijack | CWE-668 | Port stolen by another process |
| SOCK-268 | Privileged-port bind | CWE-269 | Binding < 1024 |
| SOCK-269 | Dual-stack v4/v6 exposure | CWE-668 | V4-mapped exposure |
| SOCK-270 | `IP_TRANSPARENT`/spoof options | CWE-290 | Options enabling spoof |
| SOCK-271 | TTL/IP-options abuse | CWE-20 | Crafted IP options |
| SOCK-272 | OOB/urgent data | CWE-20 | Urgent-pointer handling |
| SOCK-273 | Half-close semantics | CWE-20 | One-way shutdown |
| SOCK-274 | Multicast group-join abuse | CWE-668 | Arbitrary groups |
| SOCK-275 | Broadcast send | CWE-406 | Amplification |
| SOCK-276 | Keepalive/idle timeout absent | CWE-400 | Idle held forever |
| SOCK-277 | Connect timeout absent | CWE-400 | Blackhole hangs |
| SOCK-278 | `SO_LINGER` resource hold | CWE-404 | Linger behavior |
| SOCK-279 | PMTU blackhole | CWE-20 | Fragmentation edge |
| SOCK-280 | Port 0 / out-of-range | CWE-20 | Invalid port |
| SOCK-281 | IPv6 zone-id parse | CWE-20 | A `%25` zone id |
| SOCK-282 | Connect-refused error | CWE-20 | Clean reject |
| SOCK-283 | Address-in-use error | CWE-20 | The `EADDRINUSE` error |
| SOCK-284 | Permission-denied bind | CWE-20 | The `EACCES` error |
| SOCK-285 | Error info leak | CWE-209 | Error reveals internals |
| SOCK-286 | Exception across boundary | CWE-248 | Socket exceptions caught |
| SOCK-287 | Stack imbalance on error | CWE-664 | Error path balanced |
| SOCK-288 | Host fuzz | CWE-20 | Random hosts |
| SOCK-289 | Port fuzz | CWE-20 | Random ports |
| SOCK-290 | Payload fuzz (`send`) | CWE-20 | Random/binary payloads |
| SOCK-291 | Received-data fuzz | CWE-20 | Random bytes to a handler |
| SOCK-292 | `maxBytes` fuzz | CWE-20 | Random sizes |
| SOCK-293 | Rapid open/close churn | CWE-404 | No leak under churn |
| SOCK-294 | Integer overflow in size math | CWE-190 | Buffer-size arithmetic |
| SOCK-295 | `recvFrom` address-struct parse | CWE-20 | Malformed sender struct |
| SOCK-296 | Huge UDP `sendTo` | CWE-400 | Oversized datagram |
| SOCK-297 | Concurrent bind same addr | CWE-362 | Race binding an address |
| SOCK-298 | TLS-over-socket (if added) | CWE-295 | Cert verification |
| SOCK-299 | ASan/TSan trip under load | CWE-362 | Sanitizer finding |
| SOCK-300 | Fuzz all entry points | CWE-20 | Combined random inputs never crash |
