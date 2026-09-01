# 📁 fs

`fs.readFile(path)`, `fs.writeFile(path, content)`, `fs.exists(path)`. A general,
non-sandboxed filesystem API; path confinement is the caller's responsibility.

### Path traversal & injection

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-001 | `../` traversal read | CWE-22 | escape the intended directory |
| FS-002 | `..\` (Windows) traversal | CWE-22 | backslash separators |
| FS-003 | Absolute path | CWE-36 | `/etc/passwd` directly |
| FS-004 | UNC / network path | CWE-22 | `\\host\share` on Windows |
| FS-005 | Percent-encoded dots | CWE-22 | caller forwards `%2e%2e` undecoded |
| FS-006 | Overlong UTF-8 dots | CWE-176 | overlong `..` encodings |
| FS-007 | Nested `....//` | CWE-22 | single-strip leaves `../` |
| FS-008 | NUL truncation | CWE-626 | `secret\0.txt` truncates |
| FS-009 | Trailing dot/space | CWE-289 | `file.`/`file ` map to the same target |
| FS-010 | Case-insensitive bypass | CWE-178 | `Secret` vs blocked `secret` |
| FS-011 | Unicode normalization | CWE-178 | NFC/NFD distinct paths to one file |
| FS-012 | Reserved device names | CWE-67 | `CON`, `NUL`, `AUX` on Windows |
| FS-013 | Alternate data streams | CWE-69 | `file.txt:stream` (Windows) |
| FS-014 | Control bytes in path | CWE-74 | control chars in the path |
| FS-015 | Empty path | CWE-20 | `""` handling |
| FS-016 | Very long path | CWE-400 | path beyond `PATH_MAX` |
| FS-017 | `~` expansion assumption | CWE-22 | tilde not expanded as expected |
| FS-018 | Relative-to-cwd surprise | CWE-22 | cwd-relative resolution differs from intent |

### Symlink & TOCTOU

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-019 | Read through symlink | CWE-59 | symlink escapes the target dir |
| FS-020 | Write through symlink | CWE-59 | overwrite a symlink's target |
| FS-021 | Symlink to device | CWE-59 | symlink to `/dev/*` |
| FS-022 | Dangling symlink | CWE-59 | broken link handling |
| FS-023 | Symlink chain | CWE-59 | chained links escape |
| FS-024 | TOCTOU exists→read | CWE-367 | target swapped between calls |
| FS-025 | TOCTOU exists→write | CWE-367 | target swapped to a symlink before write |
| FS-026 | Directory swapped for file | CWE-367 | parent dir replaced mid-operation |
| FS-027 | Hardlink to protected file | CWE-59 | hardlink lets a write hit a protected inode |

### Arbitrary read/write impact

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-028 | Read sensitive config | CWE-22 | `/etc/shadow`, keys, `.env` |
| FS-029 | Read source/secrets | CWE-538 | `.git/`, backups, dotfiles |
| FS-030 | Overwrite executable | CWE-73 | clobber a binary/script |
| FS-031 | Overwrite config | CWE-73 | tamper with app config |
| FS-032 | Create file in startup dir | CWE-73 | drop a file into an autorun path |
| FS-033 | Append vs truncate semantics | CWE-20 | write mode unexpectedly truncates |
| FS-034 | Parent-dir auto-creation | CWE-22 | `writeFile` creating dirs escapes intent |
| FS-035 | Permissions of created file | CWE-276 | world-readable/writable new file |
| FS-036 | Umask/ownership surprise | CWE-732 | created file inherits broad perms |

### Resource / OOM / DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-037 | Large-file read OOM | CWE-400 | whole-file read exhausts RAM |
| FS-038 | Read at exactly the cap | CWE-193 | boundary at the size limit |
| FS-039 | File grows during read | CWE-367 | size stat then larger read |
| FS-040 | `/dev/zero` infinite read | CWE-400 | unbounded special file |
| FS-041 | FIFO/pipe read hang | CWE-400 | reading a fifo blocks forever |
| FS-042 | `/dev/random` block | CWE-400 | blocking on entropy |
| FS-043 | Huge write (disk fill) | CWE-400 | unbounded content fills disk |
| FS-044 | Many small files | CWE-400 | inode exhaustion |
| FS-045 | Deeply nested dir creation | CWE-674 | `a/a/a/...` directory creation |
| FS-046 | Sparse-file size confusion | CWE-20 | apparent vs allocated size |
| FS-047 | Slow disk blocks loop | CWE-400 | (must run off the main loop) |
| FS-048 | Concurrent reads exhaust pool | CWE-400 | many blocking reads starve the task pool |

### Binary safety & content

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-049 | NUL in content (write) | CWE-626 | content with NUL written fully |
| FS-050 | NUL in content (read) | CWE-626 | read content with NUL not truncated |
| FS-051 | Non-string content | CWE-20 | number/table content coercion |
| FS-052 | Huge content arg | CWE-400 | multi-GB content string |
| FS-053 | Invalid UTF-8 content | CWE-176 | raw bytes round-trip |
| FS-054 | Length-aware read | CWE-626 | binary file length preserved |
| FS-055 | Partial read/write | CWE-20 | short read/write handled |
| FS-056 | Encoding-agnostic | CWE-176 | no implicit transcoding |

### Errors, concurrency, fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-057 | Missing file read error | CWE-20 | clear reject, no crash |
| FS-058 | Permission-denied error | CWE-20 | EACCES surfaced cleanly |
| FS-059 | Is-a-directory read | CWE-20 | reading a directory rejects |
| FS-060 | Write to read-only path | CWE-20 | EROFS/EACCES handled |
| FS-061 | Error info leak | CWE-209 | error text reveals no internals |
| FS-062 | Promise ref leak | CWE-401 | read/write promise refs released on error |
| FS-063 | Cross-thread settle race | CWE-362 | task-pool worker settles to the loop |
| FS-064 | FD leak on error | CWE-404 | stream closed even on exceptions |
| FS-065 | Exception across boundary | CWE-248 | filesystem exceptions caught |
| FS-066 | exists() races read | CWE-367 | exists then read inconsistency |
| FS-067 | exists() on special path | CWE-20 | exists on device/fifo/symlink |
| FS-068 | Concurrent write same path | CWE-362 | interleaved writes corrupt content |
| FS-069 | Concurrent read+write | CWE-362 | torn read during a write |
| FS-070 | Path fuzz | CWE-20 | random/garbage paths never crash |
| FS-071 | Content fuzz | CWE-20 | random/binary content round-trips |
| FS-072 | Integer overflow in size | CWE-190 | size arithmetic on huge files |
| FS-073 | Reentrant fs in callback | CWE-674 | fs op from a resolve callback |
| FS-074 | Mount/quota boundary | CWE-400 | write hits a quota limit |
| FS-075 | Filename length limit | CWE-400 | name beyond `NAME_MAX` |
| FS-076 | Special chars in name | CWE-20 | newline/`*`/`?` in filename |
| FS-077 | Hidden-file handling | CWE-538 | dotfiles read/written without restriction (documented) |
| FS-078 | Atomic-write expectation | CWE-362 | write not atomic; partial on crash |
| FS-079 | Locale-dependent path | CWE-697 | path interpreted per locale |
| FS-080 | Read of a growing log | CWE-367 | size-then-read mismatch |

### Higher-level / app-context

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-081 | User input → path (no jail) | CWE-22 | request-controlled path reaches fs |
| FS-082 | Upload path from filename | CWE-434 | client filename used as a write path |
| FS-083 | Extension allow-list bypass | CWE-434 | double extension / NUL extension |
| FS-084 | Zip/archive extraction via fs | CWE-22 | combined with archive entry names |
| FS-085 | Log path traversal | CWE-22 | log filename from input |
| FS-086 | Temp-file predictability | CWE-377 | guessable temp paths |
| FS-087 | Race in temp creation | CWE-367 | predictable temp + symlink |
| FS-088 | Read of proc/sys | CWE-22 | `/proc/self/environ` leaks secrets |
| FS-089 | Write to proc/sys | CWE-22 | `/proc/sys/...` tampering |
| FS-090 | Follow mount to network FS | CWE-918 | path resolves to a network mount (SSRF-like) |
| FS-091 | Case-collision overwrite | CWE-178 | `File`/`file` overwrite on case-insensitive FS |
| FS-092 | Directory listing via read | CWE-20 | reading a dir as a file |
| FS-093 | Trailing-slash on file | CWE-20 | `file.txt/` handling |
| FS-094 | Relative `.`/`..` only | CWE-20 | `.`/`..` as the whole path |
| FS-095 | Empty content write | CWE-20 | zero-byte file |
| FS-096 | Overwrite vs create policy | CWE-20 | clobber an existing file silently |
| FS-097 | Read after delete | CWE-367 | file removed between exists and read |
| FS-098 | Disk-full write error | CWE-400 | ENOSPC handled |
| FS-099 | Path normalization parity | CWE-697 | fs vs static-handler normalize differently |
| FS-100 | Fuzz all three entry points | CWE-20 | combined random inputs never crash |

---

## Additional cases (documented attacks & CVEs)

### Traversal / naming (documented engines & platforms)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-101 | PHP null-byte truncation | CVE-class | `file.php\0.jpg` (pre-5.3.4) defeats extension checks |
| FS-102 | IIS Unicode `%c0%af` | CVE-2000-0884 | overlong-slash traversal |
| FS-103 | IIS double-decode `%255c` | CVE-2001-0333 | decoded twice to `\` |
| FS-104 | Windows 8.3 short name | CWE-66 | `SECRET~1.TXT` bypasses a name check |
| FS-105 | Windows alternate data stream | CWE-69 | `file.txt::$DATA` reveals source |
| FS-106 | Windows reserved device | CWE-67 | `CON`/`PRN`/`NUL` path |
| FS-107 | Windows trailing dot/space | CWE-289 | `secret.txt.` opens `secret.txt` |
| FS-108 | UNC path injection | CWE-22 | `\\attacker\share` triggers SMB auth leak |
| FS-109 | Drive-relative path | CWE-22 | `C:file` relative to the drive cwd |
| FS-110 | Case-insensitive FS bypass | CWE-178 | `Secret` vs blocked `secret` |
| FS-111 | macOS HFS decomposition | CWE-178 | NFD vs NFC filename |
| FS-112 | Unicode confusable filename | CWE-1007 | homoglyph name spoof |
| FS-113 | Overlong UTF-8 traversal | CWE-176 | `%c0%ae%c0%ae` |
| FS-114 | Double URL-encoded dots | CWE-22 | `%252e%252e` decoded by a layer |
| FS-115 | Nested `....//` collapse | CWE-22 | single-strip leaves `../` |
| FS-116 | Mixed separators | CWE-22 | `..\../` |
| FS-117 | Leading-slash absolute | CWE-36 | `/etc/passwd` |
| FS-118 | `~`/env expansion assumption | CWE-22 | tilde/`$HOME` expanded by a shell layer |
| FS-119 | Path with newline | CWE-74 | embedded `\n` confuses a downstream tool |
| FS-120 | Very long path (PATH_MAX) | CWE-400 | over-length path |

### Symlink / TOCTOU (documented races)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-121 | Symlink read escape | CWE-59 | symlink resolves outside the root |
| FS-122 | Symlink write escape | CWE-59 | write follows a symlink to a protected file |
| FS-123 | Symlink-swap race (TOCTOU) | CWE-367 | replace a file with a symlink mid-op |
| FS-124 | tmp-file symlink attack | CWE-377 | predictable temp + planted symlink |
| FS-125 | Hardlink to protected inode | CWE-59 | hardlink lets a write hit a protected file |
| FS-126 | Directory replaced mid-walk | CWE-367 | parent dir swapped during traversal |
| FS-127 | `O_NOFOLLOW` not used | CWE-59 | open follows a final symlink |
| FS-128 | RealPath after open | CWE-367 | validate-then-open race |
| FS-129 | Mount-point swap | CWE-367 | a mount changes the target |
| FS-130 | Dangling-symlink create | CWE-59 | creating through a broken link |

### Special files / resources (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-131 | `/proc/self/environ` read | CWE-22 | leak process env/secrets |
| FS-132 | `/proc/self/maps` read | CWE-22 | leak memory layout |
| FS-133 | `/proc/self/mem` access | CWE-22 | read/write process memory |
| FS-134 | `/dev/zero` infinite read | CWE-400 | unbounded read |
| FS-135 | `/dev/random` blocking read | CWE-400 | entropy starvation hang |
| FS-136 | FIFO read deadlock | CWE-833 | open a fifo with no writer |
| FS-137 | Device file read/write | CWE-67 | `/dev/sda` raw access |
| FS-138 | `/sys` tunable write | CWE-22 | kernel tunable tampering |
| FS-139 | Network mount path | CWE-918 | path resolves to NFS/SMB (SSRF-like) |
| FS-140 | Sparse-file apparent size | CWE-130 | stat size != allocated |

### Permissions / integrity (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-141 | World-writable created file | CWE-732 | broad permissions on new file |
| FS-142 | World-readable secret file | CWE-732 | secret written 0644 |
| FS-143 | umask not enforced | CWE-276 | inherited permissive umask |
| FS-144 | setuid/setgid bit on create | CWE-732 | dangerous mode bits |
| FS-145 | Ownership not set | CWE-282 | wrong owner on created file |
| FS-146 | Non-atomic write | CWE-362 | partial file on crash |
| FS-147 | Missing fsync durability | CWE-662 | data loss on power failure |
| FS-148 | Backup/temp left behind | CWE-459 | `.tmp`/`~` leftover with secrets |
| FS-149 | Overwrite without backup | CWE-494 | clobber + no rollback |
| FS-150 | Predictable temp name | CWE-377 | guessable temp path |

### Resource / OOM / DoS (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-151 | Large file read (no cap) | CWE-400 | reads any size, like Node; bounded only by memory (trusted local code) |
| FS-152 | File grows during read | CWE-367 | size-then-read mismatch |
| FS-153 | Disk fill via write | CWE-400 | ENOSPC handling |
| FS-154 | Inode exhaustion | CWE-400 | many tiny files |
| FS-155 | Quota exhaustion | CWE-400 | write hits a quota |
| FS-156 | Deeply nested dir create | CWE-674 | `a/a/a/...` |
| FS-157 | Recursive-traversal symlink loop | CWE-835 | symlink cycle in a walk |
| FS-158 | Blocking read on slow FS | CWE-400 | network FS stalls a worker |
| FS-159 | Task-pool starvation | CWE-400 | many blocking reads |
| FS-160 | FD leak on error path | CWE-404 | stream not closed on throw |

### Binding / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-161 | NUL in path | CWE-626 | `path\0.png` truncation |
| FS-162 | NUL in content (write) | CWE-626 | content with NUL written fully |
| FS-163 | NUL in content (read) | CWE-626 | NUL not truncating the result |
| FS-164 | Non-string args | CWE-20 | number/table path/content |
| FS-165 | Huge content arg | CWE-400 | multi-GB content |
| FS-166 | Invalid-UTF8 content round-trip | CWE-176 | raw bytes preserved |
| FS-167 | exists() TOCTOU | CWE-367 | exists then read mismatch |
| FS-168 | exists() on special path | CWE-20 | device/fifo/symlink |
| FS-169 | Concurrent write same path | CWE-362 | interleaved writes corrupt |
| FS-170 | Concurrent read+write | CWE-362 | torn read during a write |
| FS-171 | Cross-thread settle race | CWE-362 | worker settles to the loop |
| FS-172 | Promise ref leak | CWE-401 | op refs not released on error |
| FS-173 | Exception across boundary | CWE-248 | filesystem exceptions caught |
| FS-174 | Stack imbalance on error | CWE-664 | error path balanced |
| FS-175 | Integer overflow in size math | CWE-190 | size arithmetic on huge files |
| FS-176 | Reentrant fs in callback | CWE-674 | fs op from a resolve callback |
| FS-177 | Path fuzz | CWE-20 | random/garbage paths never crash |
| FS-178 | Content fuzz | CWE-20 | random/binary content round-trips |
| FS-179 | Error-message info leak | CWE-209 | error reveals absolute paths |
| FS-180 | ASan trip on path handling | CWE-125 | OOB in path manipulation |

### App-context / misuse (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-181 | User-input path (no jail) | CWE-22 | request-controlled path |
| FS-182 | Upload write from filename | CWE-434 | client filename as a write path |
| FS-183 | Extension allow-list bypass | CWE-434 | double/NUL extension |
| FS-184 | Log path traversal | CWE-22 | log filename from input |
| FS-185 | Config write from input | CWE-73 | tamper with app config |
| FS-186 | Code/script overwrite | CWE-94 | clobber a loaded `.lua` |
| FS-187 | Startup-dir file drop | CWE-73 | drop into an autorun path |
| FS-188 | Archive-extract path (with zip) | CWE-22 | combined with entry names |
| FS-189 | Path normalization parity (vs static) | CWE-697 | fs vs static normalize differently |
| FS-190 | Read after delete | CWE-367 | removed between exists and read |
| FS-191 | Case-collision overwrite | CWE-178 | `File`/`file` on case-insensitive FS |
| FS-192 | Directory read as file | CWE-20 | reading a directory |
| FS-193 | Trailing-slash on a file | CWE-20 | `file.txt/` |
| FS-194 | `.`/`..` as whole path | CWE-20 | dot-only path |
| FS-195 | Empty content write | CWE-20 | zero-byte file |
| FS-196 | Silent overwrite policy | CWE-20 | clobber an existing file |
| FS-197 | Symlink in extraction root | CWE-59 | a created parent is a symlink |
| FS-198 | Locale-dependent path | CWE-697 | path interpreted per locale |
| FS-199 | Differential dummy/std driver | CWE-697 | stub vs real storage diverge |
| FS-200 | Fuzz all three entry points | CWE-20 | combined random inputs never crash |

---

## Round 3 — deeper / documented

### Traversal / naming (deeper platform quirks)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-201 | Windows `\\?\` long-path prefix | CWE-22 | extended-length path bypass |
| FS-202 | Windows `\\.\` device namespace | CWE-67 | device path access |
| FS-203 | Windows trailing-dot strip | CWE-289 | `secret.txt.` opens the file |
| FS-204 | Windows space-strip | CWE-289 | `secret.txt ` opens the file |
| FS-205 | Windows colon ADS | CWE-69 | `file:stream` data stream |
| FS-206 | Windows short-name 8.3 | CWE-66 | `SECRET~1` aliasing |
| FS-207 | macOS `/..namedfork/rsrc` | CWE-22 | resource-fork access |
| FS-208 | macOS HFS NFD filename | CWE-178 | decomposition mismatch |
| FS-209 | Linux `//` collapse assumption | CWE-22 | double-slash handling |
| FS-210 | `.` / `..` only path | CWE-22 | dot-only components |
| FS-211 | Trailing slash on a file | CWE-20 | `file.txt/` |
| FS-212 | Mixed `/` and `\` | CWE-22 | separator confusion |
| FS-213 | Percent-decoded path forwarded | CWE-22 | request decoding reaches fs |
| FS-214 | Overlong UTF-8 traversal | CWE-176 | `%c0%ae` dots |
| FS-215 | Unicode-confusable filename | CWE-1007 | homoglyph name |
| FS-216 | Control bytes in filename | CWE-74 | newline/`*`/`?` in a name |
| FS-217 | Case-collision overwrite | CWE-178 | `File`/`file` on case-insensitive FS |
| FS-218 | Reserved Windows device name | CWE-67 | `CON`/`NUL`/`AUX` |
| FS-219 | NUL truncation extension | CWE-626 | `file.txt%00.png` |
| FS-220 | Absolute UNC path | CWE-22 | `\\host\share` |

### Symlink / link / TOCTOU (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-221 | Final-component symlink (no O_NOFOLLOW) | CWE-59 | open follows the last link |
| FS-222 | Intermediate-component symlink | CWE-59 | a parent dir is a symlink |
| FS-223 | Symlink-swap between stat and open | CWE-367 | replace target mid-op |
| FS-224 | Validate-then-open race | CWE-367 | realpath then open |
| FS-225 | Mount-point swap | CWE-367 | target mount changes |
| FS-226 | Hardlink to protected inode | CWE-59 | write hits a protected file |
| FS-227 | tmp-file symlink attack | CWE-377 | predictable temp + planted link |
| FS-228 | Directory-fd reuse | CWE-367 | openat base changes |
| FS-229 | Symlink loop | CWE-835 | circular links |
| FS-230 | Dangling-symlink create | CWE-59 | create through a broken link |

### Special files / resources (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-231 | `/proc/self/environ` read | CWE-22 | env/secret disclosure |
| FS-232 | `/proc/self/cmdline` read | CWE-22 | args disclosure |
| FS-233 | `/proc/self/maps` read | CWE-22 | memory layout leak |
| FS-234 | `/proc/self/fd/*` read | CWE-22 | reach other open files |
| FS-235 | `/dev/zero` infinite read | CWE-400 | unbounded read |
| FS-236 | `/dev/random` blocking | CWE-400 | entropy starvation |
| FS-237 | FIFO read deadlock | CWE-833 | no writer |
| FS-238 | Character/block device | CWE-67 | raw device access |
| FS-239 | `/sys` tunable write | CWE-22 | kernel tunable tamper |
| FS-240 | Network-mount target | CWE-918 | path resolves to NFS/SMB |
| FS-241 | Sparse-file apparent size | CWE-130 | stat vs allocated |
| FS-242 | Socket file (AF_UNIX) | CWE-67 | opening a socket path |

### Permissions / integrity / resource (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-243 | World-writable created file | CWE-732 | broad permissions |
| FS-244 | World-readable secret | CWE-732 | secret written 0644 |
| FS-245 | umask not enforced | CWE-276 | permissive inherited umask |
| FS-246 | setuid/setgid on create | CWE-732 | dangerous mode bits |
| FS-247 | Ownership not set | CWE-282 | wrong owner |
| FS-248 | Non-atomic write | CWE-362 | partial file on crash |
| FS-249 | Missing fsync durability | CWE-662 | data loss |
| FS-250 | Backup/temp left behind | CWE-459 | leftover with secrets |
| FS-251 | Predictable temp name | CWE-377 | guessable temp |
| FS-252 | Large file read boundary | CWE-193 | a large file reads fully; no artificial size cap |
| FS-253 | File grows during read | CWE-367 | size-then-read mismatch |
| FS-254 | Disk fill via write | CWE-400 | ENOSPC handling |
| FS-255 | Inode exhaustion | CWE-400 | many tiny files |
| FS-256 | Quota exhaustion | CWE-400 | write hits a quota |
| FS-257 | Filename length limit | CWE-400 | beyond NAME_MAX |
| FS-258 | Path length limit | CWE-400 | beyond PATH_MAX |

### Binding / concurrency / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-259 | NUL in path | CWE-626 | path truncation |
| FS-260 | NUL in content (write) | CWE-626 | written fully |
| FS-261 | NUL in content (read) | CWE-626 | not truncated |
| FS-262 | Non-string args | CWE-20 | number/table args |
| FS-263 | Huge content arg | CWE-400 | multi-GB content |
| FS-264 | Invalid-UTF8 round-trip | CWE-176 | raw bytes preserved |
| FS-265 | exists() TOCTOU | CWE-367 | exists then read mismatch |
| FS-266 | exists() on special path | CWE-20 | device/fifo/symlink |
| FS-267 | Concurrent write same path | CWE-362 | interleaved writes corrupt |
| FS-268 | Concurrent read+write | CWE-362 | torn read |
| FS-269 | Cross-thread settle race | CWE-362 | worker settles to the loop |
| FS-270 | Promise ref leak | CWE-401 | refs not released on error |
| FS-271 | Exception across boundary | CWE-248 | exceptions caught |
| FS-272 | Stack imbalance on error | CWE-664 | error path balanced |
| FS-273 | FD leak on error | CWE-404 | stream closed on throw |
| FS-274 | Integer overflow in size math | CWE-190 | size arithmetic |
| FS-275 | Reentrant fs in callback | CWE-674 | fs op from a resolve callback |
| FS-276 | Path fuzz | CWE-20 | random paths never crash |
| FS-277 | Content fuzz | CWE-20 | random content round-trips |
| FS-278 | Error-message info leak | CWE-209 | absolute paths in errors |
| FS-279 | ASan trip on path handling | CWE-125 | OOB in manipulation |
| FS-280 | Task-pool starvation | CWE-400 | many blocking reads |

### App-context / misuse (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-281 | User-input path (no jail) | CWE-22 | request-controlled path |
| FS-282 | Upload write from client filename | CWE-434 | filename as a write path |
| FS-283 | Double/NUL extension bypass | CWE-434 | allow-list evasion |
| FS-284 | Log path traversal | CWE-22 | log filename from input |
| FS-285 | Config write from input | CWE-73 | config tamper |
| FS-286 | Loaded-script overwrite | CWE-94 | clobber a `.lua` |
| FS-287 | Autorun-dir file drop | CWE-73 | drop into a startup path |
| FS-288 | Archive-extract path combo | CWE-22 | with zip entry names |
| FS-289 | Read of proc/sys secrets | CWE-22 | env/keys via virtual fs |
| FS-290 | Write to proc/sys | CWE-22 | tunable tamper |
| FS-291 | Mount-redirect SSRF | CWE-918 | path resolves to a network mount |
| FS-292 | Read after delete | CWE-367 | removed between exists and read |
| FS-293 | Directory read as file | CWE-20 | reading a directory |
| FS-294 | Empty-content write | CWE-20 | zero-byte file |
| FS-295 | Silent overwrite policy | CWE-20 | clobber an existing file |
| FS-296 | Symlink in extraction root | CWE-59 | created parent is a symlink |
| FS-297 | Locale-dependent path | CWE-697 | locale changes interpretation |
| FS-298 | Path normalization parity (vs static) | CWE-697 | fs vs static differ |
| FS-299 | Differential dummy/std driver | CWE-697 | stub vs real diverge |
| FS-300 | Fuzz all three entry points | CWE-20 | combined random inputs never crash |
