# 📁 fs

Covers `fs.readFile(path)`, `fs.writeFile(path, content)`, `fs.exists(path)`. A general, non-sandboxed filesystem API. Path confinement is the caller's responsibility.

### Path traversal & injection

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-001 | `../` traversal read | CWE-22 | Escape the intended directory |
| FS-002 | `..\` (Windows) traversal | CWE-22 | Backslash separators |
| FS-003 | Absolute path | CWE-36 | Reading `/etc/passwd` directly |
| FS-004 | UNC / network path | CWE-22 | A path like `\\host\share` on Windows |
| FS-005 | Percent-encoded dots | CWE-22 | Caller forwards `%2e%2e` undecoded |
| FS-006 | Overlong UTF-8 dots | CWE-176 | Overlong `..` encodings |
| FS-007 | Nested `....//` | CWE-22 | Single-strip leaves `../` |
| FS-008 | NUL truncation | CWE-626 | A name like `secret\0.txt` truncates |
| FS-009 | Trailing dot/space | CWE-289 | Names `file. `/`file ` map to the same target |
| FS-010 | Case-insensitive bypass | CWE-178 | Requesting `Secret` vs blocked `secret` |
| FS-011 | Unicode normalization | CWE-178 | NFC/NFD distinct paths to one file |
| FS-012 | Reserved device names | CWE-67 | Names `CON`, `NUL`, `AUX` on Windows |
| FS-013 | Alternate data streams | CWE-69 | A stream name like `file.txt:stream` (Windows) |
| FS-014 | Control bytes in path | CWE-74 | Control chars in the path |
| FS-015 | Empty path | CWE-20 | Handling of `""` |
| FS-016 | Very long path | CWE-400 | Path beyond `PATH_MAX` |
| FS-017 | `~` expansion assumption | CWE-22 | Tilde not expanded as expected |
| FS-018 | Relative-to-cwd surprise | CWE-22 | Resolution relative to the cwd differs from intent |

### Symlink & TOCTOU

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-019 | Read through symlink | CWE-59 | Symlink escapes the target dir |
| FS-020 | Write through symlink | CWE-59 | Overwrite a symlink's target |
| FS-021 | Symlink to device | CWE-59 | Symlink to `/dev/*` |
| FS-022 | Dangling symlink | CWE-59 | Broken link handling |
| FS-023 | Symlink chain | CWE-59 | Chained links escape |
| FS-024 | TOCTOU `exists`→read | CWE-367 | Target swapped between calls |
| FS-025 | TOCTOU `exists`→write | CWE-367 | Target swapped to a symlink before write |
| FS-026 | Directory swapped for file | CWE-367 | Parent dir replaced mid-operation |
| FS-027 | Hardlink to protected file | CWE-59 | Hardlink lets a write hit a protected inode |

### Arbitrary read/write impact

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-028 | Read sensitive config | CWE-22 | Files like `/etc/shadow`, keys, `.env` |
| FS-029 | Read source/secrets | CWE-538 | Contents like `.git/`, backups, dotfiles |
| FS-030 | Overwrite executable | CWE-73 | Clobber a binary/script |
| FS-031 | Overwrite config | CWE-73 | Tamper with app config |
| FS-032 | Create file in startup dir | CWE-73 | Drop a file into an autorun path |
| FS-033 | Append vs truncate semantics | CWE-20 | Write mode unexpectedly truncates |
| FS-034 | Parent-dir auto-creation | CWE-22 | A `writeFile` call creating dirs escapes intent |
| FS-035 | Permissions of created file | CWE-276 | World-readable/writable new file |
| FS-036 | Umask/ownership surprise | CWE-732 | Created file inherits broad perms |

### Resource / OOM / DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-037 | Large-file read OOM | CWE-400 | Whole-file read exhausts RAM |
| FS-038 | Read at exactly the cap | CWE-193 | Boundary at the size limit |
| FS-039 | File grows during read | CWE-367 | Size `stat` then larger read |
| FS-040 | `/dev/zero` infinite read | CWE-400 | Unbounded special file |
| FS-041 | FIFO/pipe read hang | CWE-400 | Reading a fifo blocks forever |
| FS-042 | `/dev/random` block | CWE-400 | Blocking on entropy |
| FS-043 | Huge write (disk fill) | CWE-400 | Unbounded content fills disk |
| FS-044 | Many small files | CWE-400 | Inode exhaustion |
| FS-045 | Deeply nested dir creation | CWE-674 | Directory creation along `a/a/a/...` |
| FS-046 | Sparse-file size confusion | CWE-20 | Apparent vs allocated size |
| FS-047 | Slow disk blocks loop | CWE-400 | Must run off the main loop |
| FS-048 | Concurrent reads exhaust pool | CWE-400 | Many blocking reads starve the task pool |

### Binary safety & content

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-049 | NUL in content (write) | CWE-626 | Content with NUL written fully |
| FS-050 | NUL in content (read) | CWE-626 | Read content with NUL not truncated |
| FS-051 | Non-string content | CWE-20 | Number/table content coercion |
| FS-052 | Huge content arg | CWE-400 | Multi-GB content string |
| FS-053 | Invalid UTF-8 content | CWE-176 | Raw bytes round-trip |
| FS-054 | Length-aware read | CWE-626 | Binary file length preserved |
| FS-055 | Partial read/write | CWE-20 | Short read/write handled |
| FS-056 | Encoding-agnostic | CWE-176 | No implicit transcoding |

### Errors, concurrency, fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-057 | Missing file read error | CWE-20 | Clear reject, no crash |
| FS-058 | Permission-denied error | CWE-20 | An `EACCES` surfaced cleanly |
| FS-059 | Is-a-directory read | CWE-20 | Reading a directory rejects |
| FS-060 | Write to read-only path | CWE-20 | Errors `EROFS`/`EACCES` handled |
| FS-061 | Error info leak | CWE-209 | Error text reveals no internals |
| FS-062 | Promise ref leak | CWE-401 | Read/write promise refs released on error |
| FS-063 | Cross-thread settle race | CWE-362 | Task-pool worker settles to the loop |
| FS-064 | FD leak on error | CWE-404 | Stream closed even on exceptions |
| FS-065 | Exception across boundary | CWE-248 | Filesystem exceptions caught |
| FS-066 | `exists()` races read | CWE-367 | An `exists` then read inconsistency |
| FS-067 | `exists()` on special path | CWE-20 | Calling `exists` on device/fifo/symlink |
| FS-068 | Concurrent write same path | CWE-362 | Interleaved writes corrupt content |
| FS-069 | Concurrent read+write | CWE-362 | Torn read during a write |
| FS-070 | Path fuzz | CWE-20 | Random/garbage paths never crash |
| FS-071 | Content fuzz | CWE-20 | Random/binary content round-trips |
| FS-072 | Integer overflow in size | CWE-190 | Size arithmetic on huge files |
| FS-073 | Reentrant fs in callback | CWE-674 | An `fs` op from a resolve callback |
| FS-074 | Mount/quota boundary | CWE-400 | Write hits a quota limit |
| FS-075 | Filename length limit | CWE-400 | Name beyond `NAME_MAX` |
| FS-076 | Special chars in name | CWE-20 | Newline/`*`/`?` in filename |
| FS-077 | Hidden-file handling | CWE-538 | Dotfiles read/written without restriction (documented) |
| FS-078 | Atomic-write expectation | CWE-362 | Write not atomic. Partial on crash |
| FS-079 | Locale-dependent path | CWE-697 | Path interpreted per locale |
| FS-080 | Read of a growing log | CWE-367 | Size-then-read mismatch |

### Higher-level / app-context

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-081 | User input → path (no jail) | CWE-22 | Request-controlled path reaches fs |
| FS-082 | Upload path from filename | CWE-434 | Client filename used as a write path |
| FS-083 | Extension allow-list bypass | CWE-434 | Double extension / NUL extension |
| FS-084 | Zip/archive extraction via fs | CWE-22 | Combined with archive entry names |
| FS-085 | Log path traversal | CWE-22 | Log filename from input |
| FS-086 | Temp-file predictability | CWE-377 | Guessable temp paths |
| FS-087 | Race in temp creation | CWE-367 | Predictable temp + symlink |
| FS-088 | Read of proc/sys | CWE-22 | Reading `/proc/self/environ` leaks secrets |
| FS-089 | Write to proc/sys | CWE-22 | Tampering with `/proc/sys/...` |
| FS-090 | Follow mount to network FS | CWE-918 | Path resolves to a network mount (SSRF-like) |
| FS-091 | Case-collision overwrite | CWE-178 | A `File`/`file` overwrite on case-insensitive FS |
| FS-092 | Directory listing via read | CWE-20 | Reading a dir as a file |
| FS-093 | Trailing-slash on file | CWE-20 | Handling of `file.txt/` |
| FS-094 | Relative `.`/`..` only | CWE-20 | Using `.`/`..` as the whole path |
| FS-095 | Empty content write | CWE-20 | Zero-byte file |
| FS-096 | Overwrite vs create policy | CWE-20 | Clobber an existing file silently |
| FS-097 | Read after delete | CWE-367 | File removed between `exists` and read |
| FS-098 | Disk-full write error | CWE-400 | The `ENOSPC` error handled |
| FS-099 | Path normalization parity | CWE-697 | The `fs` module vs static-handler normalize differently |
| FS-100 | Fuzz all three entry points | CWE-20 | Combined random inputs never crash |

---

## Additional cases (documented attacks & CVEs)

### Traversal / naming (documented engines & platforms)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-101 | PHP null-byte truncation | CVE-class | A name like `file.php\0.jpg` (pre-5.3.4) defeats extension checks |
| FS-102 | IIS Unicode `%c0%af` | CVE-2000-0884 | Overlong-slash traversal |
| FS-103 | IIS double-decode `%255c` | CVE-2001-0333 | Decoded twice to `\` |
| FS-104 | Windows 8.3 short name | CWE-66 | A short name like `SECRET~1.TXT` bypasses a name check |
| FS-105 | Windows alternate data stream | CWE-69 | A request for `file.txt::$DATA` reveals source |
| FS-106 | Windows reserved device | CWE-67 | A `CON`/`PRN`/`NUL` path |
| FS-107 | Windows trailing dot/space | CWE-289 | A request for `secret.txt. ` opens `secret.txt` |
| FS-108 | UNC path injection | CWE-22 | A path like `\\attacker\share` triggers SMB auth leak |
| FS-109 | Drive-relative path | CWE-22 | A path like `C:file` relative to the drive cwd |
| FS-110 | Case-insensitive FS bypass | CWE-178 | Requesting `Secret` vs blocked `secret` |
| FS-111 | macOS HFS decomposition | CWE-178 | NFD vs NFC filename |
| FS-112 | Unicode confusable filename | CWE-1007 | Homoglyph name spoof |
| FS-113 | Overlong UTF-8 traversal | CWE-176 | The sequence `%c0%ae%c0%ae` |
| FS-114 | Double URL-encoded dots | CWE-22 | The sequence `%252e%252e` decoded by a layer |
| FS-115 | Nested `....//` collapse | CWE-22 | Single-strip leaves `../` |
| FS-116 | Mixed separators | CWE-22 | The mixed path `..\../` |
| FS-117 | Leading-slash absolute | CWE-36 | The absolute path `/etc/passwd` |
| FS-118 | `~`/env expansion assumption | CWE-22 | Tilde/`$HOME` expanded by a shell layer |
| FS-119 | Path with newline | CWE-74 | Embedded `\n` confuses a downstream tool |
| FS-120 | Very long path (`PATH_MAX`) | CWE-400 | Over-length path |

### Symlink / TOCTOU (documented races)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-121 | Symlink read escape | CWE-59 | Symlink resolves outside the root |
| FS-122 | Symlink write escape | CWE-59 | Write follows a symlink to a protected file |
| FS-123 | Symlink-swap race (TOCTOU) | CWE-367 | Replace a file with a symlink mid-op |
| FS-124 | tmp-file symlink attack | CWE-377 | Predictable temp + planted symlink |
| FS-125 | Hardlink to protected inode | CWE-59 | Hardlink lets a write hit a protected file |
| FS-126 | Directory replaced mid-walk | CWE-367 | Parent dir swapped during traversal |
| FS-127 | `O_NOFOLLOW` not used | CWE-59 | The `open` call follows a final symlink |
| FS-128 | RealPath after open | CWE-367 | Validate-then-open race |
| FS-129 | Mount-point swap | CWE-367 | A mount changes the target |
| FS-130 | Dangling-symlink create | CWE-59 | Creating through a broken link |

### Special files / resources (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-131 | `/proc/self/environ` read | CWE-22 | Leak process env/secrets |
| FS-132 | `/proc/self/maps` read | CWE-22 | Leak memory layout |
| FS-133 | `/proc/self/mem` access | CWE-22 | Read/write process memory |
| FS-134 | `/dev/zero` infinite read | CWE-400 | Unbounded read |
| FS-135 | `/dev/random` blocking read | CWE-400 | Entropy starvation hang |
| FS-136 | FIFO read deadlock | CWE-833 | Open a fifo with no writer |
| FS-137 | Device file read/write | CWE-67 | Raw access to `/dev/sda` |
| FS-138 | `/sys` tunable write | CWE-22 | Kernel tunable tampering |
| FS-139 | Network mount path | CWE-918 | Path resolves to NFS/SMB (SSRF-like) |
| FS-140 | Sparse-file apparent size | CWE-130 | The `stat` size != allocated |

### Permissions / integrity (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-141 | World-writable created file | CWE-732 | Broad permissions on new file |
| FS-142 | World-readable secret file | CWE-732 | Secret written `0644` |
| FS-143 | umask not enforced | CWE-276 | Inherited permissive umask |
| FS-144 | setuid/setgid bit on create | CWE-732 | Dangerous mode bits |
| FS-145 | Ownership not set | CWE-282 | Wrong owner on created file |
| FS-146 | Non-atomic write | CWE-362 | Partial file on crash |
| FS-147 | Missing fsync durability | CWE-662 | Data loss on power failure |
| FS-148 | Backup/temp left behind | CWE-459 | A `.tmp`/`~` leftover with secrets |
| FS-149 | Overwrite without backup | CWE-494 | Clobber + no rollback |
| FS-150 | Predictable temp name | CWE-377 | Guessable temp path |

### Resource / OOM / DoS (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-151 | Large file read (no cap) | CWE-400 | Reads any size, like Node. Bounded only by memory (trusted local code) |
| FS-152 | File grows during read | CWE-367 | Size-then-read mismatch |
| FS-153 | Disk fill via write | CWE-400 | Handling of `ENOSPC` |
| FS-154 | Inode exhaustion | CWE-400 | Many tiny files |
| FS-155 | Quota exhaustion | CWE-400 | Write hits a quota |
| FS-156 | Deeply nested dir create | CWE-674 | A path like `a/a/a/...` |
| FS-157 | Recursive-traversal symlink loop | CWE-835 | Symlink cycle in a walk |
| FS-158 | Blocking read on slow FS | CWE-400 | Network FS stalls a worker |
| FS-159 | Task-pool starvation | CWE-400 | Many blocking reads |
| FS-160 | FD leak on error path | CWE-404 | Stream not closed on throw |

### Binding / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-161 | NUL in path | CWE-626 | Truncation through `path\0.png` |
| FS-162 | NUL in content (write) | CWE-626 | Content with NUL written fully |
| FS-163 | NUL in content (read) | CWE-626 | NUL not truncating the result |
| FS-164 | Non-string args | CWE-20 | Number/table path/content |
| FS-165 | Huge content arg | CWE-400 | Multi-GB content |
| FS-166 | Invalid-UTF8 content round-trip | CWE-176 | Raw bytes preserved |
| FS-167 | `exists()` TOCTOU | CWE-367 | An `exists` then read mismatch |
| FS-168 | `exists()` on special path | CWE-20 | Device/fifo/symlink |
| FS-169 | Concurrent write same path | CWE-362 | Interleaved writes corrupt |
| FS-170 | Concurrent read+write | CWE-362 | Torn read during a write |
| FS-171 | Cross-thread settle race | CWE-362 | Worker settles to the loop |
| FS-172 | Promise ref leak | CWE-401 | Op refs not released on error |
| FS-173 | Exception across boundary | CWE-248 | Filesystem exceptions caught |
| FS-174 | Stack imbalance on error | CWE-664 | Error path balanced |
| FS-175 | Integer overflow in size math | CWE-190 | Size arithmetic on huge files |
| FS-176 | Reentrant fs in callback | CWE-674 | An `fs` op from a resolve callback |
| FS-177 | Path fuzz | CWE-20 | Random/garbage paths never crash |
| FS-178 | Content fuzz | CWE-20 | Random/binary content round-trips |
| FS-179 | Error-message info leak | CWE-209 | Error reveals absolute paths |
| FS-180 | ASan trip on path handling | CWE-125 | OOB in path manipulation |

### App-context / misuse (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-181 | User-input path (no jail) | CWE-22 | Request-controlled path |
| FS-182 | Upload write from filename | CWE-434 | Client filename as a write path |
| FS-183 | Extension allow-list bypass | CWE-434 | Double/NUL extension |
| FS-184 | Log path traversal | CWE-22 | Log filename from input |
| FS-185 | Config write from input | CWE-73 | Tamper with app config |
| FS-186 | Code/script overwrite | CWE-94 | Clobber a loaded `.lua` |
| FS-187 | Startup-dir file drop | CWE-73 | Drop into an autorun path |
| FS-188 | Archive-extract path (with zip) | CWE-22 | Combined with entry names |
| FS-189 | Path normalization parity (vs static) | CWE-697 | The `fs` module vs static normalize differently |
| FS-190 | Read after delete | CWE-367 | Removed between `exists` and read |
| FS-191 | Case-collision overwrite | CWE-178 | Names `File`/`file` on case-insensitive FS |
| FS-192 | Directory read as file | CWE-20 | Reading a directory |
| FS-193 | Trailing-slash on a file | CWE-20 | A path like `file.txt/` |
| FS-194 | `.`/`..` as whole path | CWE-20 | Dot-only path |
| FS-195 | Empty content write | CWE-20 | Zero-byte file |
| FS-196 | Silent overwrite policy | CWE-20 | Clobber an existing file |
| FS-197 | Symlink in extraction root | CWE-59 | A created parent is a symlink |
| FS-198 | Locale-dependent path | CWE-697 | Path interpreted per locale |
| FS-199 | Differential dummy/std driver | CWE-697 | Stub vs real storage diverge |
| FS-200 | Fuzz all three entry points | CWE-20 | Combined random inputs never crash |

---

## Round 3 — deeper / documented

### Traversal / naming (deeper platform quirks)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-201 | Windows `\\?\` long-path prefix | CWE-22 | Extended-length path bypass |
| FS-202 | Windows `\\.\` device namespace | CWE-67 | Device path access |
| FS-203 | Windows trailing-dot strip | CWE-289 | A request for `secret.txt. ` opens the file |
| FS-204 | Windows space-strip | CWE-289 | A request for `secret.txt ` opens the file |
| FS-205 | Windows colon ADS | CWE-69 | A `file:stream` data stream |
| FS-206 | Windows short-name 8.3 | CWE-66 | Aliasing through `SECRET~1` |
| FS-207 | macOS `/..namedfork/rsrc` | CWE-22 | Resource-fork access |
| FS-208 | macOS HFS NFD filename | CWE-178 | Decomposition mismatch |
| FS-209 | Linux `//` collapse assumption | CWE-22 | Double-slash handling |
| FS-210 | `.` / `..` only path | CWE-22 | Dot-only components |
| FS-211 | Trailing slash on a file | CWE-20 | A path like `file.txt/` |
| FS-212 | Mixed `/` and `\` | CWE-22 | Separator confusion |
| FS-213 | Percent-decoded path forwarded | CWE-22 | Request decoding reaches fs |
| FS-214 | Overlong UTF-8 traversal | CWE-176 | Dots encoded as `%c0%ae` |
| FS-215 | Unicode-confusable filename | CWE-1007 | Homoglyph name |
| FS-216 | Control bytes in filename | CWE-74 | Newline/`*`/`?` in a name |
| FS-217 | Case-collision overwrite | CWE-178 | Names `File`/`file` on case-insensitive FS |
| FS-218 | Reserved Windows device name | CWE-67 | Names `CON`/`NUL`/`AUX` |
| FS-219 | NUL truncation extension | CWE-626 | A name like `file.txt%00.png` |
| FS-220 | Absolute UNC path | CWE-22 | A path like `\\host\share` |

### Symlink / link / TOCTOU (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-221 | Final-component symlink (no `O_NOFOLLOW`) | CWE-59 | The `open` call follows the last link |
| FS-222 | Intermediate-component symlink | CWE-59 | A parent dir is a symlink |
| FS-223 | Symlink-swap between stat and open | CWE-367 | Replace target mid-op |
| FS-224 | Validate-then-open race | CWE-367 | Calling `realpath` then `open` |
| FS-225 | Mount-point swap | CWE-367 | Target mount changes |
| FS-226 | Hardlink to protected inode | CWE-59 | Write hits a protected file |
| FS-227 | tmp-file symlink attack | CWE-377 | Predictable temp + planted link |
| FS-228 | Directory-fd reuse | CWE-367 | The `openat` base changes |
| FS-229 | Symlink loop | CWE-835 | Circular links |
| FS-230 | Dangling-symlink create | CWE-59 | Create through a broken link |

### Special files / resources (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-231 | `/proc/self/environ` read | CWE-22 | Env/secret disclosure |
| FS-232 | `/proc/self/cmdline` read | CWE-22 | Args disclosure |
| FS-233 | `/proc/self/maps` read | CWE-22 | Memory layout leak |
| FS-234 | `/proc/self/fd/*` read | CWE-22 | Reach other open files |
| FS-235 | `/dev/zero` infinite read | CWE-400 | Unbounded read |
| FS-236 | `/dev/random` blocking | CWE-400 | Entropy starvation |
| FS-237 | FIFO read deadlock | CWE-833 | No writer |
| FS-238 | Character/block device | CWE-67 | Raw device access |
| FS-239 | `/sys` tunable write | CWE-22 | Kernel tunable tamper |
| FS-240 | Network-mount target | CWE-918 | Path resolves to NFS/SMB |
| FS-241 | Sparse-file apparent size | CWE-130 | The `stat` size vs allocated |
| FS-242 | Socket file (`AF_UNIX`) | CWE-67 | Opening a socket path |

### Permissions / integrity / resource (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-243 | World-writable created file | CWE-732 | Broad permissions |
| FS-244 | World-readable secret | CWE-732 | Secret written `0644` |
| FS-245 | umask not enforced | CWE-276 | Permissive inherited umask |
| FS-246 | setuid/setgid on create | CWE-732 | Dangerous mode bits |
| FS-247 | Ownership not set | CWE-282 | Wrong owner |
| FS-248 | Non-atomic write | CWE-362 | Partial file on crash |
| FS-249 | Missing fsync durability | CWE-662 | Data loss |
| FS-250 | Backup/temp left behind | CWE-459 | Leftover with secrets |
| FS-251 | Predictable temp name | CWE-377 | Guessable temp |
| FS-252 | Large file read boundary | CWE-193 | A large file reads fully. No artificial size cap |
| FS-253 | File grows during read | CWE-367 | Size-then-read mismatch |
| FS-254 | Disk fill via write | CWE-400 | Handling of `ENOSPC` |
| FS-255 | Inode exhaustion | CWE-400 | Many tiny files |
| FS-256 | Quota exhaustion | CWE-400 | Write hits a quota |
| FS-257 | Filename length limit | CWE-400 | Beyond `NAME_MAX` |
| FS-258 | Path length limit | CWE-400 | Beyond `PATH_MAX` |

### Binding / concurrency / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-259 | NUL in path | CWE-626 | Path truncation |
| FS-260 | NUL in content (write) | CWE-626 | Written fully |
| FS-261 | NUL in content (read) | CWE-626 | Not truncated |
| FS-262 | Non-string args | CWE-20 | Number/table args |
| FS-263 | Huge content arg | CWE-400 | Multi-GB content |
| FS-264 | Invalid-UTF8 round-trip | CWE-176 | Raw bytes preserved |
| FS-265 | `exists()` TOCTOU | CWE-367 | An `exists` then read mismatch |
| FS-266 | `exists()` on special path | CWE-20 | Device/fifo/symlink |
| FS-267 | Concurrent write same path | CWE-362 | Interleaved writes corrupt |
| FS-268 | Concurrent read+write | CWE-362 | Torn read |
| FS-269 | Cross-thread settle race | CWE-362 | Worker settles to the loop |
| FS-270 | Promise ref leak | CWE-401 | Refs not released on error |
| FS-271 | Exception across boundary | CWE-248 | Exceptions caught |
| FS-272 | Stack imbalance on error | CWE-664 | Error path balanced |
| FS-273 | FD leak on error | CWE-404 | Stream closed on throw |
| FS-274 | Integer overflow in size math | CWE-190 | Size arithmetic |
| FS-275 | Reentrant fs in callback | CWE-674 | An `fs` op from a resolve callback |
| FS-276 | Path fuzz | CWE-20 | Random paths never crash |
| FS-277 | Content fuzz | CWE-20 | Random content round-trips |
| FS-278 | Error-message info leak | CWE-209 | Absolute paths in errors |
| FS-279 | ASan trip on path handling | CWE-125 | OOB in manipulation |
| FS-280 | Task-pool starvation | CWE-400 | Many blocking reads |

### App-context / misuse (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FS-281 | User-input path (no jail) | CWE-22 | Request-controlled path |
| FS-282 | Upload write from client filename | CWE-434 | Filename as a write path |
| FS-283 | Double/NUL extension bypass | CWE-434 | Allow-list evasion |
| FS-284 | Log path traversal | CWE-22 | Log filename from input |
| FS-285 | Config write from input | CWE-73 | Config tamper |
| FS-286 | Loaded-script overwrite | CWE-94 | Clobber a `.lua` |
| FS-287 | Autorun-dir file drop | CWE-73 | Drop into a startup path |
| FS-288 | Archive-extract path combo | CWE-22 | With zip entry names |
| FS-289 | Read of proc/sys secrets | CWE-22 | Env/keys via virtual fs |
| FS-290 | Write to proc/sys | CWE-22 | Tunable tamper |
| FS-291 | Mount-redirect SSRF | CWE-918 | Path resolves to a network mount |
| FS-292 | Read after delete | CWE-367 | Removed between `exists` and read |
| FS-293 | Directory read as file | CWE-20 | Reading a directory |
| FS-294 | Empty-content write | CWE-20 | Zero-byte file |
| FS-295 | Silent overwrite policy | CWE-20 | Clobber an existing file |
| FS-296 | Symlink in extraction root | CWE-59 | Created parent is a symlink |
| FS-297 | Locale-dependent path | CWE-697 | Locale changes interpretation |
| FS-298 | Path normalization parity (vs static) | CWE-697 | The `fs` module vs static differ |
| FS-299 | Differential dummy/std driver | CWE-697 | Stub vs real diverge |
| FS-300 | Fuzz all three entry points | CWE-20 | Combined random inputs never crash |
