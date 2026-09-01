# 🗜️ zip

`zip.create(archive, entries)`, `zip.extract(archive, destDir)`, `zip.list(archive)`
(libzip-backed, async).

### Zip slip / path traversal on extract

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-001 | Relative traversal | CWE-22 | entry `../../evil` escapes destDir |
| ZIP-002 | Absolute path entry | CWE-36 | entry `/etc/cron.d/x` |
| ZIP-003 | Windows absolute (`C:\`) | CWE-36 | drive-letter path |
| ZIP-004 | Backslash separators | CWE-22 | `..\..\evil` |
| ZIP-005 | Mixed separators | CWE-22 | `..\../evil` |
| ZIP-006 | Deeply nested `..` | CWE-22 | `a/../../../etc/x` |
| ZIP-007 | Percent-like / encoded name | CWE-22 | encoded dot sequences in the entry name |
| ZIP-008 | Overlong UTF-8 dots | CWE-176 | overlong `..` in the name |
| ZIP-009 | Trailing dot/space (Windows) | CWE-289 | `evil.`/`evil ` maps elsewhere |
| ZIP-010 | NUL in entry name | CWE-626 | name truncation at NUL |
| ZIP-011 | Canonicalization bypass | CWE-22 | name normalizes outside destRoot |
| ZIP-012 | destDir is a symlink | CWE-59 | extraction root itself is a symlink |
| ZIP-013 | Parent created as symlink | CWE-59 | a created parent dir is hijacked |
| ZIP-014 | Case-collision overwrite | CWE-178 | `File`/`file` collide on case-insensitive FS |
| ZIP-015 | Reserved device name entry | CWE-67 | `CON`/`NUL` entry on Windows |

### Symlink / special entries

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-016 | Symlink entry escapes root | CWE-59 | a symlink member points outside |
| ZIP-017 | Symlink + follow-up write | CWE-59 | later entry writes through the symlink |
| ZIP-018 | Hardlink entry | CWE-59 | hardlink to a protected inode |
| ZIP-019 | Device/FIFO entry | CWE-67 | special-file member created |
| ZIP-020 | Setuid/permission bits | CWE-732 | entry carries dangerous mode bits |
| ZIP-021 | Directory-vs-file confusion | CWE-20 | entry name with/without trailing `/` |
| ZIP-022 | Empty entry name | CWE-20 | zero-length name |
| ZIP-023 | Entry name only `.`/`..` | CWE-22 | dot-only names |

### Decompression bombs / resource

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-024 | Classic zip bomb | CWE-409 | tiny archive expands to gigabytes |
| ZIP-025 | Total-size cap | CWE-409 | total extracted bytes bounded |
| ZIP-026 | Per-entry size cap | CWE-409 | one huge entry bounded |
| ZIP-027 | Entry-count flood | CWE-400 | millions of entries (inodes/CPU) |
| ZIP-028 | Nested zip (zip-in-zip) | CWE-409 | recursive archives (single-level extract) |
| ZIP-029 | Zip quine | CWE-674 | self-referential archive |
| ZIP-030 | High compression ratio | CWE-409 | extreme deflate ratio |
| ZIP-031 | Overlapping entries | CWE-409 | overlapping local-header data |
| ZIP-032 | Disk exhaustion | CWE-400 | extraction fills the disk |
| ZIP-033 | Deeply nested directories | CWE-674 | `a/a/a/...` paths |
| ZIP-034 | Huge entry name | CWE-400 | multi-MB entry name |
| ZIP-035 | Huge comment field | CWE-400 | giant archive/entry comment |
| ZIP-036 | Many tiny files | CWE-400 | inode/CPU exhaustion |
| ZIP-037 | Memory amplification on list | CWE-400 | listing a huge central directory |

### Archive structure / format fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-038 | Corrupt central directory | CWE-20 | malformed CD rejected, no crash |
| ZIP-039 | Mismatched local/central headers | CWE-20 | conflicting metadata |
| ZIP-040 | Bad CRC | CWE-354 | wrong CRC detected |
| ZIP-041 | Truncated archive | CWE-20 | cut-off file handled |
| ZIP-042 | Garbage prepended/appended | CWE-20 | data around the archive |
| ZIP-043 | Wrong magic | CWE-20 | non-zip input rejected |
| ZIP-044 | ZIP64 extremes | CWE-190 | 64-bit sizes/counts |
| ZIP-045 | Encrypted entry | CWE-20 | password-protected member handling |
| ZIP-046 | Unsupported compression method | CWE-20 | unknown method rejected |
| ZIP-047 | Negative/huge declared size | CWE-190 | size field overflow |
| ZIP-048 | Extra-field abuse | CWE-20 | malformed extra fields |
| ZIP-049 | Data-descriptor confusion | CWE-20 | streaming descriptor mismatch |
| ZIP-050 | Filename encoding flag | CWE-176 | UTF-8 flag vs raw bytes |
| ZIP-051 | Mojibake entry names | CWE-176 | non-UTF8 names |
| ZIP-052 | Duplicate entry names | CWE-20 | two members with the same name |
| ZIP-053 | libzip CVE surface | CWE-1395 | keep libzip patched |
| ZIP-054 | Format fuzz corpus | CWE-20 | random/mutated archives never crash |

### Create path

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-055 | Unsafe entry name on create | CWE-22 | `../` entry rejected at create |
| ZIP-056 | Absolute entry on create | CWE-36 | absolute name rejected |
| ZIP-057 | Source path traversal | CWE-22 | `file` path escapes intent |
| ZIP-058 | Source is a symlink | CWE-59 | following a symlink source |
| ZIP-059 | Source is a special file | CWE-20 | adding a device/fifo |
| ZIP-060 | Source is a directory | CWE-20 | non-regular-file source rejected |
| ZIP-061 | Huge source file | CWE-400 | adding a multi-GB file |
| ZIP-062 | NUL in entry name (create) | CWE-626 | name truncation |
| ZIP-063 | Empty entries list | CWE-20 | create with no entries |
| ZIP-064 | Overwrite existing archive | CWE-20 | clobber semantics on create |
| ZIP-065 | Archive path traversal | CWE-22 | the archive path itself escapes |
| ZIP-066 | Many entries on create | CWE-400 | creating a huge archive |
| ZIP-067 | Entry name fuzz (create) | CWE-20 | random names handled |

### Memory / concurrency / errors

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-068 | `zip_t` leak on error | CWE-401 | archive discarded on every error path |
| ZIP-069 | `zip_file_t` leak | CWE-401 | entry handle closed on error |
| ZIP-070 | Output file leak | CWE-404 | extracted-file stream closed on error |
| ZIP-071 | Buffer handling in read loop | CWE-120 | 8 KB buffer bounds |
| ZIP-072 | OOM mid-extract | CWE-400 | allocation failure handled |
| ZIP-073 | OOM mid-list | CWE-400 | listing allocation failure |
| ZIP-074 | Integer overflow in size math | CWE-190 | total/entry size arithmetic |
| ZIP-075 | Negative size cast | CWE-197 | `zip_int64_t` to size cast |
| ZIP-076 | Exception across boundary | CWE-248 | libzip errors caught |
| ZIP-077 | Stack imbalance on error | CWE-664 | error path leaves a balanced Lua stack |
| ZIP-078 | Promise ref leak | CWE-401 | op promise refs released |
| ZIP-079 | Cross-thread settle race | CWE-362 | worker settles to the loop |
| ZIP-080 | Concurrent extract same dir | CWE-362 | two extractions race in destDir |
| ZIP-081 | Task-pool starvation | CWE-400 | many extracts exhaust the pool |
| ZIP-082 | TOCTOU on destDir | CWE-367 | destDir swapped during extraction |
| ZIP-083 | TOCTOU on entry path | CWE-367 | parent dir swapped before write |
| ZIP-084 | Partial extraction on error | CWE-459 | leftover files after a mid-extract failure |
| ZIP-085 | Error info leak | CWE-209 | error text reveals paths/internals |
| ZIP-086 | Reentrant op in callback | CWE-674 | zip op from a resolve callback |

### Higher-level / misc

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-087 | Entry name from user input | CWE-22 | request-controlled entry name |
| ZIP-088 | Archive from upload | CWE-434 | extracting an untrusted upload |
| ZIP-089 | destDir from user input | CWE-22 | request-controlled destination |
| ZIP-090 | Extraction outside app root | CWE-22 | escaping the app's data dir |
| ZIP-091 | Overwrite app files | CWE-73 | extraction clobbers code/config |
| ZIP-092 | Symlink-then-write ordering | CWE-59 | ordered entries to hijack a write |
| ZIP-093 | Time-of-extract permissions | CWE-732 | extracted file modes too broad |
| ZIP-094 | Path length limit | CWE-400 | entry path beyond `PATH_MAX` |
| ZIP-095 | Unicode normalization names | CWE-178 | NFC/NFD collisions |
| ZIP-096 | Control bytes in names | CWE-74 | control chars in entry names |
| ZIP-097 | Listing trusts entry names | CWE-20 | `list` returns unsanitized names |
| ZIP-098 | Empty archive | CWE-20 | zero-entry archive handled |
| ZIP-099 | Repeated extract idempotence | CWE-367 | re-extraction overwrites/races |
| ZIP-100 | Fuzz create+extract+list | CWE-20 | combined random inputs never crash |

---

## Additional cases (documented attacks & CVEs)

### Zip slip / traversal (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-101 | Snyk Zip Slip (2018) class | CWE-22 | `../` entry escapes on extract |
| ZIP-102 | evilarc-style payload | CWE-22 | tool-generated traversal archive |
| ZIP-103 | Deep `../` chain | CWE-22 | `../../../../../etc/x` |
| ZIP-104 | Absolute Unix path | CWE-36 | `/etc/cron.d/x` |
| ZIP-105 | Windows drive path | CWE-36 | `C:\Windows\...` |
| ZIP-106 | Backslash on Unix | CWE-22 | `..\` interpreted as a name |
| ZIP-107 | Mixed-separator | CWE-22 | `..\../` |
| ZIP-108 | Trailing dot/space entry | CWE-289 | Windows name normalization |
| ZIP-109 | NUL in entry name | CWE-626 | name truncation |
| ZIP-110 | Reserved-device entry | CWE-67 | `CON`/`NUL` member |
| ZIP-111 | 8.3 short-name collision | CWE-66 | short-name aliasing |
| ZIP-112 | Unicode-normalization name | CWE-178 | NFC/NFD collision |
| ZIP-113 | Overlong UTF-8 dots | CWE-176 | overlong `..` |
| ZIP-114 | Canonicalization-after-join bypass | CWE-22 | name normalizes outside destRoot |
| ZIP-115 | destRoot is a symlink | CWE-59 | extraction root is a symlink |
| ZIP-116 | Created-parent symlink | CWE-59 | a parent dir is a planted symlink |

### Symlink / link entries (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-117 | Symlink member escape | CWE-59 | symlink points outside root |
| ZIP-118 | Symlink-then-write ordering | CWE-59 | a later entry writes through the symlink |
| ZIP-119 | Hardlink to protected file | CWE-59 | hardlink entry to a protected inode |
| ZIP-120 | Symlink loop | CWE-835 | circular symlinks |
| ZIP-121 | Dangling symlink create | CWE-59 | broken-link member |
| ZIP-122 | Device/FIFO member | CWE-67 | special-file entry created |
| ZIP-123 | Dangerous mode bits | CWE-732 | setuid/world-writable member |
| ZIP-124 | Directory-vs-file confusion | CWE-20 | trailing-`/` ambiguity |
| ZIP-125 | TOCTOU on entry parent | CWE-367 | parent swapped before write |

### Bombs / resource (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-126 | 42.zip recursive bomb | CWE-409 | classic nested-zip bomb |
| ZIP-127 | Flat high-ratio bomb | CWE-409 | single huge deflate stream |
| ZIP-128 | Overlapping-entry bomb | CWE-409 | entries share compressed data (David Fifield) |
| ZIP-129 | Zip quine | CWE-674 | archive contains itself |
| ZIP-130 | Total-size cap boundary | CWE-409 | at/over the 2 GiB cap |
| ZIP-131 | Per-entry size cap | CWE-409 | one entry exceeds the cap |
| ZIP-132 | Entry-count cap boundary | CWE-400 | at/over the 100k cap |
| ZIP-133 | Inode exhaustion | CWE-400 | millions of tiny files |
| ZIP-134 | Disk-fill on extract | CWE-400 | extraction exhausts disk |
| ZIP-135 | Deeply nested directories | CWE-674 | `a/a/a/...` member paths |
| ZIP-136 | Huge comment field | CWE-400 | giant archive/entry comment |
| ZIP-137 | Huge entry name | CWE-400 | multi-MB name |
| ZIP-138 | Memory blowup on list | CWE-400 | huge central directory |
| ZIP-139 | Compression-ratio guard | CWE-409 | declared vs actual size mismatch |

### Format / parser (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-140 | ZIP64 size/count extremes | CWE-190 | 64-bit fields overflow |
| ZIP-141 | Corrupt central directory | CWE-20 | malformed CD |
| ZIP-142 | Local/central header mismatch | CWE-20 | conflicting metadata |
| ZIP-143 | Bad CRC detection | CWE-354 | wrong CRC |
| ZIP-144 | Truncated archive | CWE-20 | cut-off file |
| ZIP-145 | Prepended/appended data | CWE-20 | self-extracting stub / junk |
| ZIP-146 | Wrong magic | CWE-20 | non-zip input |
| ZIP-147 | Encrypted entry | CWE-20 | password-protected member |
| ZIP-148 | Unsupported compression method | CWE-20 | unknown method |
| ZIP-149 | Negative/huge declared size | CWE-190 | size field overflow |
| ZIP-150 | Extra-field abuse | CWE-20 | malformed extra fields |
| ZIP-151 | Data-descriptor confusion | CWE-20 | streaming descriptor mismatch |
| ZIP-152 | UTF-8 flag vs raw bytes | CWE-176 | filename encoding flag |
| ZIP-153 | Mojibake entry names | CWE-176 | non-UTF8 names |
| ZIP-154 | Duplicate entry names | CWE-20 | two members, same name |
| ZIP-155 | libzip CVE surface | CWE-1395 | keep libzip patched |
| ZIP-156 | Spanned/multi-disk archive | CWE-20 | multi-volume handling |
| ZIP-157 | Format fuzz corpus | CWE-20 | mutated archives never crash |

### Create path (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-158 | Unsafe entry name on create | CWE-22 | `../` rejected at create |
| ZIP-159 | Absolute entry on create | CWE-36 | absolute name rejected |
| ZIP-160 | Source path traversal | CWE-22 | source path escapes |
| ZIP-161 | Symlink source | CWE-59 | following a symlink source |
| ZIP-162 | Special-file source | CWE-20 | device/fifo source |
| ZIP-163 | Directory source | CWE-20 | non-regular-file source |
| ZIP-164 | Huge source file | CWE-400 | multi-GB source |
| ZIP-165 | NUL in entry name (create) | CWE-626 | name truncation |
| ZIP-166 | Empty entries list | CWE-20 | create with no entries |
| ZIP-167 | Overwrite existing archive | CWE-20 | clobber semantics |
| ZIP-168 | Archive path traversal | CWE-22 | the archive path escapes |
| ZIP-169 | Entry-name fuzz (create) | CWE-20 | random names handled |

### Memory / concurrency / errors (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-170 | `zip_t` leak on error | CWE-401 | archive discarded on every error |
| ZIP-171 | `zip_file_t` leak | CWE-401 | entry handle closed on error |
| ZIP-172 | Output-file leak | CWE-404 | extracted stream closed on error |
| ZIP-173 | Read-loop buffer bounds | CWE-120 | fixed buffer handling |
| ZIP-174 | OOM mid-extract | CWE-400 | allocation failure |
| ZIP-175 | OOM mid-list | CWE-400 | listing allocation failure |
| ZIP-176 | Integer overflow in size math | CWE-190 | total/entry arithmetic |
| ZIP-177 | Negative size cast | CWE-197 | `zip_int64_t` to size |
| ZIP-178 | Exception across boundary | CWE-248 | libzip errors caught |
| ZIP-179 | Stack imbalance on error | CWE-664 | error path balanced |
| ZIP-180 | Promise ref leak | CWE-401 | op refs released |
| ZIP-181 | Cross-thread settle race | CWE-362 | worker settles to the loop |
| ZIP-182 | Concurrent extract same dir | CWE-362 | two extractions race |
| ZIP-183 | Task-pool starvation | CWE-400 | many extracts exhaust the pool |
| ZIP-184 | TOCTOU on destDir | CWE-367 | destDir swapped mid-extract |
| ZIP-185 | Partial extraction on error | CWE-459 | leftover files after failure |
| ZIP-186 | Error info leak | CWE-209 | error reveals paths |
| ZIP-187 | Reentrant op in callback | CWE-674 | zip op from a resolve callback |
| ZIP-188 | ASan trip on malformed archive | CWE-125 | OOB read in parsing |

### App-context / misc (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-189 | Entry name from user input | CWE-22 | request-controlled name |
| ZIP-190 | Extract an untrusted upload | CWE-434 | user-supplied archive |
| ZIP-191 | destDir from user input | CWE-22 | request-controlled destination |
| ZIP-192 | Extraction outside app root | CWE-22 | escaping the data dir |
| ZIP-193 | Overwrite app code/config | CWE-73 | clobber files via extraction |
| ZIP-194 | Permissions after extract | CWE-732 | extracted modes too broad |
| ZIP-195 | Path length limit | CWE-400 | entry path beyond PATH_MAX |
| ZIP-196 | Control bytes in names | CWE-74 | control chars in names |
| ZIP-197 | `list` returns raw names | CWE-20 | unsanitized names to the caller |
| ZIP-198 | Empty archive | CWE-20 | zero-entry archive |
| ZIP-199 | Re-extract idempotence/race | CWE-367 | overwrite/race on re-extraction |
| ZIP-200 | Fuzz create+extract+list | CWE-20 | combined random inputs never crash |

---

## Round 3 — deeper / documented

### Zip slip / traversal (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-201 | Encoded-dot entry name | CWE-22 | `%2e%2e` style in a name |
| ZIP-202 | Double-encoded entry name | CWE-22 | `%252e%252e` |
| ZIP-203 | Nested `....//` entry | CWE-22 | single-strip leaves `../` |
| ZIP-204 | Mixed-separator entry | CWE-22 | `..\../` |
| ZIP-205 | Drive-relative (Windows) | CWE-36 | `C:evil` |
| ZIP-206 | UNC entry path | CWE-22 | `\\host\share` |
| ZIP-207 | Reserved-device entry | CWE-67 | `CON`/`NUL` member |
| ZIP-208 | Trailing dot/space entry | CWE-289 | Windows normalization |
| ZIP-209 | NUL in entry name | CWE-626 | name truncation |
| ZIP-210 | Overlong UTF-8 dots | CWE-176 | overlong `..` |
| ZIP-211 | Unicode-normalization collision | CWE-178 | NFC/NFD names |
| ZIP-212 | 8.3 short-name collision | CWE-66 | short-name alias |
| ZIP-213 | Canonicalize-after-join bypass | CWE-22 | name escapes destRoot |
| ZIP-214 | Control bytes in name | CWE-74 | control chars |
| ZIP-215 | Very long entry path | CWE-400 | beyond PATH_MAX |

### Links / special entries (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-216 | Symlink member escape | CWE-59 | points outside root |
| ZIP-217 | Symlink-then-write ordering | CWE-59 | later entry writes through it |
| ZIP-218 | destRoot is a symlink | CWE-59 | extraction root is a link |
| ZIP-219 | Created-parent symlink | CWE-59 | planted parent dir |
| ZIP-220 | Hardlink to protected inode | CWE-59 | hardlink member |
| ZIP-221 | Device/FIFO member | CWE-67 | special-file entry |
| ZIP-222 | Dangerous mode bits | CWE-732 | setuid/world-writable member |
| ZIP-223 | Symlink loop | CWE-835 | circular link members |
| ZIP-224 | Directory-vs-file confusion | CWE-20 | trailing-`/` ambiguity |
| ZIP-225 | TOCTOU on entry parent | CWE-367 | parent swapped before write |

### Bombs / resource (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-226 | 42.zip recursive bomb | CWE-409 | nested-zip expansion |
| ZIP-227 | Flat high-ratio bomb | CWE-409 | single huge deflate |
| ZIP-228 | Overlapping-entry bomb | CWE-409 | shared compressed data |
| ZIP-229 | Zip quine | CWE-674 | self-referential archive |
| ZIP-230 | Total-size cap boundary | CWE-409 | at/over the 2 GiB cap |
| ZIP-231 | Per-entry size cap | CWE-409 | one huge entry |
| ZIP-232 | Entry-count cap boundary | CWE-400 | at/over the 100k cap |
| ZIP-233 | Inode exhaustion | CWE-400 | millions of files |
| ZIP-234 | Disk-fill on extract | CWE-400 | exhaust disk |
| ZIP-235 | Deeply nested directories | CWE-674 | `a/a/a/...` |
| ZIP-236 | Huge comment field | CWE-400 | giant comment |
| ZIP-237 | Huge entry name | CWE-400 | multi-MB name |
| ZIP-238 | List memory blowup | CWE-400 | huge central directory |
| ZIP-239 | Compression-ratio guard | CWE-409 | declared vs actual mismatch |

### Format / parser (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-240 | ZIP64 extremes | CWE-190 | 64-bit field overflow |
| ZIP-241 | Corrupt central directory | CWE-20 | malformed CD |
| ZIP-242 | Local/central mismatch | CWE-20 | conflicting metadata |
| ZIP-243 | Bad CRC detection | CWE-354 | wrong CRC |
| ZIP-244 | Truncated archive | CWE-20 | cut-off file |
| ZIP-245 | Prepended/appended data | CWE-20 | SFX stub / junk |
| ZIP-246 | Wrong magic | CWE-20 | non-zip input |
| ZIP-247 | Encrypted entry | CWE-20 | password-protected member |
| ZIP-248 | Unsupported method | CWE-20 | unknown compression |
| ZIP-249 | Negative/huge declared size | CWE-190 | size overflow |
| ZIP-250 | Extra-field abuse | CWE-20 | malformed extra fields |
| ZIP-251 | Data-descriptor confusion | CWE-20 | streaming descriptor mismatch |
| ZIP-252 | UTF-8 flag vs raw bytes | CWE-176 | filename encoding flag |
| ZIP-253 | Mojibake entry names | CWE-176 | non-UTF8 names |
| ZIP-254 | Duplicate entry names | CWE-20 | same name twice |
| ZIP-255 | Spanned/multi-disk archive | CWE-20 | multi-volume |
| ZIP-256 | libzip/zlib CVE surface | CWE-1395 | keep deps patched |
| ZIP-257 | Format fuzz corpus | CWE-20 | mutated archives never crash |

### Create path (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-258 | Unsafe entry name on create | CWE-22 | `../` rejected |
| ZIP-259 | Absolute entry on create | CWE-36 | absolute rejected |
| ZIP-260 | Source path traversal | CWE-22 | source escapes |
| ZIP-261 | Symlink source | CWE-59 | following a symlink source |
| ZIP-262 | Special-file source | CWE-20 | device/fifo source |
| ZIP-263 | Directory source | CWE-20 | non-regular source |
| ZIP-264 | Huge source file | CWE-400 | multi-GB source |
| ZIP-265 | NUL in entry name (create) | CWE-626 | name truncation |
| ZIP-266 | Empty entries list | CWE-20 | no entries |
| ZIP-267 | Overwrite existing archive | CWE-20 | clobber semantics |
| ZIP-268 | Archive path traversal | CWE-22 | archive path escapes |
| ZIP-269 | Entry-name fuzz (create) | CWE-20 | random names |

### Memory / concurrency / errors (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-270 | `zip_t` leak on error | CWE-401 | discarded on every error |
| ZIP-271 | `zip_file_t` leak | CWE-401 | handle closed on error |
| ZIP-272 | Output-file leak | CWE-404 | stream closed on error |
| ZIP-273 | Read-loop buffer bounds | CWE-120 | fixed buffer handling |
| ZIP-274 | OOM mid-extract | CWE-400 | allocation failure |
| ZIP-275 | OOM mid-list | CWE-400 | listing allocation failure |
| ZIP-276 | Integer overflow in size math | CWE-190 | total/entry arithmetic |
| ZIP-277 | Negative size cast | CWE-197 | `zip_int64_t` to size |
| ZIP-278 | Exception across boundary | CWE-248 | libzip errors caught |
| ZIP-279 | Stack imbalance on error | CWE-664 | error path balanced |
| ZIP-280 | Promise ref leak | CWE-401 | op refs released |
| ZIP-281 | Cross-thread settle race | CWE-362 | worker settles to the loop |
| ZIP-282 | Concurrent extract same dir | CWE-362 | two extractions race |
| ZIP-283 | Task-pool starvation | CWE-400 | many extracts exhaust the pool |
| ZIP-284 | TOCTOU on destDir | CWE-367 | destDir swapped mid-extract |
| ZIP-285 | Partial extraction on error | CWE-459 | leftover files |
| ZIP-286 | Error info leak | CWE-209 | error reveals paths |
| ZIP-287 | Reentrant op in callback | CWE-674 | zip op from a callback |
| ZIP-288 | ASan trip on malformed archive | CWE-125 | OOB read |

### App-context / misc (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ZIP-289 | Entry name from user input | CWE-22 | request-controlled name |
| ZIP-290 | Extract an untrusted upload | CWE-434 | user-supplied archive |
| ZIP-291 | destDir from user input | CWE-22 | request-controlled destination |
| ZIP-292 | Extraction outside app root | CWE-22 | escaping the data dir |
| ZIP-293 | Overwrite app code/config | CWE-73 | clobber files |
| ZIP-294 | Permissions after extract | CWE-732 | extracted modes too broad |
| ZIP-295 | Path length limit | CWE-400 | beyond PATH_MAX |
| ZIP-296 | Control bytes in names | CWE-74 | control chars |
| ZIP-297 | `list` returns raw names | CWE-20 | unsanitized to the caller |
| ZIP-298 | Empty archive | CWE-20 | zero-entry archive |
| ZIP-299 | Re-extract idempotence/race | CWE-367 | overwrite/race on re-extraction |
| ZIP-300 | Fuzz create+extract+list | CWE-20 | combined random inputs never crash |
