# 🖥️ platform

`platform.os/arch/hostVersion/cpuCount/pointerSize/endianness/libPrefix/shlibSuffix`,
`platform.libraryFilename(name)`, `platform.getLibraryPathByName(name, subdir?)`.

### Information disclosure

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-001 | OS disclosure | CWE-200 | `os()` aids fingerprinting/exploit selection |
| PLAT-002 | Arch disclosure | CWE-200 | `arch()` narrows payloads |
| PLAT-003 | Version disclosure | CWE-200 | `hostVersion()` reveals patch level |
| PLAT-004 | CPU count disclosure | CWE-200 | `cpuCount()` infra detail |
| PLAT-005 | Pointer size disclosure | CWE-200 | `pointerSize()` 32/64-bit hint |
| PLAT-006 | Endianness disclosure | CWE-200 | `endianness()` exploit tuning |
| PLAT-007 | Library naming disclosure | CWE-200 | prefix/suffix reveal the platform |
| PLAT-008 | Absolute path disclosure | CWE-209 | `getLibraryPathByName` leaks internal paths |
| PLAT-009 | Field leaked to HTTP client | CWE-200 | platform info echoed in a response |
| PLAT-010 | Field leaked in logs | CWE-532 | platform info logged |
| PLAT-011 | Build-time secret in version | CWE-540 | version string embeds build details |
| PLAT-012 | Env-derived value leak | CWE-200 | value sourced from a sensitive env var |
| PLAT-013 | Fingerprint composite | CWE-200 | combined fields uniquely identify the host |
| PLAT-014 | Timing/feature inference | CWE-203 | behavior reveals platform features |
| PLAT-015 | Hostname/identity leak | CWE-200 | any host identifier exposed |

### `libraryFilename` / `getLibraryPathByName` — path building

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-016 | Traversal in name | CWE-22 | `../../evil` as the library name |
| PLAT-017 | Traversal in subdir | CWE-22 | `../../` as the subdir |
| PLAT-018 | Absolute name | CWE-36 | `/etc/passwd` as the name |
| PLAT-019 | Absolute subdir | CWE-36 | absolute subdir path |
| PLAT-020 | NUL in name | CWE-626 | truncation at NUL |
| PLAT-021 | NUL in subdir | CWE-626 | subdir truncation |
| PLAT-022 | Control bytes in name | CWE-74 | control chars in the built path |
| PLAT-023 | Backslash separators | CWE-22 | `..\` on Windows |
| PLAT-024 | Empty name | CWE-20 | `""` name handling |
| PLAT-025 | Empty subdir | CWE-20 | `""`/nil subdir handling |
| PLAT-026 | Very long name | CWE-400 | multi-MB name |
| PLAT-027 | Very long subdir | CWE-400 | multi-MB subdir |
| PLAT-028 | Name with separators | CWE-22 | embedded `/` in the name |
| PLAT-029 | Built path used by FFI/loader | CWE-426 | result fed to `dlopen` enables hijack |
| PLAT-030 | Unicode/overlong in name | CWE-176 | overlong UTF-8 in the name |
| PLAT-031 | Trailing dot/space (Windows) | CWE-289 | name normalization quirks |
| PLAT-032 | Filename injection of extension | CWE-20 | name controls the resulting extension |
| PLAT-033 | Subdir with `..%2f` | CWE-22 | encoded traversal in subdir |
| PLAT-034 | Result path normalization | CWE-22 | composed path escapes the intended dir |
| PLAT-035 | Non-string name/subdir | CWE-20 | number/table arguments |

### Argument validation & robustness

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-036 | Missing required arg | CWE-20 | `libraryFilename()` with no name |
| PLAT-037 | Extra arguments | CWE-20 | superfluous args ignored safely |
| PLAT-038 | Wrong arg type | CWE-20 | boolean/function as the name |
| PLAT-039 | nil argument | CWE-20 | nil name/subdir |
| PLAT-040 | Huge argument | CWE-400 | gigantic strings |
| PLAT-041 | Binary/NUL argument | CWE-626 | length-aware handling |
| PLAT-042 | Invalid UTF-8 argument | CWE-176 | raw bytes in args |
| PLAT-043 | Locale-affected output | CWE-697 | locale changes formatting |
| PLAT-044 | Return type stability | CWE-20 | every function returns the documented type |
| PLAT-045 | Output non-empty guarantee | CWE-20 | `os()`/`arch()` never empty |
| PLAT-046 | Semver format guarantee | CWE-20 | `hostVersion()` matches `x.y.z` |
| PLAT-047 | cpuCount ≥ 1 | CWE-20 | never zero/negative |
| PLAT-048 | pointerSize ∈ {4,8} | CWE-20 | only valid values |
| PLAT-049 | endianness ∈ {little,big} | CWE-20 | only valid values |
| PLAT-050 | Stable across calls | CWE-20 | values are consistent within a run |

### Memory / binding / concurrency

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-051 | Buffer handling in path build | CWE-120 | string concatenation bounds |
| PLAT-052 | Integer overflow in size math | CWE-190 | length arithmetic on huge args |
| PLAT-053 | Stack imbalance on error | CWE-664 | error path leaves a balanced Lua stack |
| PLAT-054 | NUL-truncated returns | CWE-626 | returned strings use length |
| PLAT-055 | Exception across boundary | CWE-248 | any throw contained |
| PLAT-056 | Concurrent calls | CWE-362 | thread-safe, no shared mutable global |
| PLAT-057 | Static-init value cache race | CWE-362 | cached value computed once safely |
| PLAT-058 | Reentrant call via metatable | CWE-674 | re-entry during arg coercion |
| PLAT-059 | Memory leak under churn | CWE-401 | repeated calls do not leak |
| PLAT-060 | Output buffer reuse | CWE-664 | a shared scratch buffer races |
| PLAT-061 | Return ref lifetime | CWE-416 | returned C string lifetime |
| PLAT-062 | Uninitialized field read | CWE-457 | a platform field read before set |
| PLAT-063 | Huge return amplification | CWE-400 | pathological return sizes |
| PLAT-064 | Type confusion in args | CWE-843 | userdata passed where string expected |
| PLAT-065 | Fuzz path builders | CWE-20 | random name/subdir never crash |

### Trust / supply-chain / misuse

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-066 | Trusting os()/arch() for security | CWE-807 | branching security on a spoofable value |
| PLAT-067 | Library path → loader hijack | CWE-426 | built path enables a planted library |
| PLAT-068 | Subdir from user input | CWE-22 | request-controlled subdir |
| PLAT-069 | Name from user input | CWE-22 | request-controlled library name |
| PLAT-070 | Path used without validation | CWE-20 | callers trust the composed path |
| PLAT-071 | Version-gated logic bypass | CWE-693 | a version check is spoofable |
| PLAT-072 | Feature detection spoof | CWE-807 | feature flag trusted from this module |
| PLAT-073 | Endianness assumption bug | CWE-188 | code branches wrongly on endianness |
| PLAT-074 | Pointer-size assumption | CWE-188 | layout assumptions from `pointerSize` |
| PLAT-075 | Path separator assumption | CWE-20 | hardcoded `/` vs platform separator |
| PLAT-076 | shlibSuffix mismatch | CWE-20 | wrong extension for the platform |
| PLAT-077 | libPrefix mismatch | CWE-20 | wrong `lib` prefix |
| PLAT-078 | Cross-compile value mismatch | CWE-20 | build-host vs target values differ |
| PLAT-079 | WASM platform reporting | CWE-20 | `wasm`/`wasm32` correctness |
| PLAT-080 | Android/iOS reporting | CWE-20 | mobile identifiers correct |
| PLAT-081 | Windows reporting | CWE-20 | windows identifiers correct |
| PLAT-082 | Unknown-platform fallback | CWE-20 | graceful unknown handling |
| PLAT-083 | CPU count from cgroup | CWE-20 | container CPU limits vs host count |
| PLAT-084 | hostVersion injection | CWE-117 | version string with control bytes |
| PLAT-085 | Version comparison logic | CWE-697 | string vs semver comparison bugs |
| PLAT-086 | Path traversal parity with fs | CWE-697 | platform path vs fs normalize differently |
| PLAT-087 | Output cached stale | CWE-20 | a changed environment not reflected |
| PLAT-088 | Disclosure via error timing | CWE-203 | error timing reveals platform |
| PLAT-089 | Library filename collision | CWE-706 | two names map to one filename |
| PLAT-090 | Subdir absolute-override | CWE-22 | absolute subdir ignores base |
| PLAT-091 | Path with mixed separators | CWE-22 | `a\b/c` mixed |
| PLAT-092 | Trailing-separator handling | CWE-20 | `subdir/` double-slash result |
| PLAT-093 | Empty result possibility | CWE-20 | builder returns an empty path |
| PLAT-094 | Non-ASCII filename | CWE-176 | unicode library names |
| PLAT-095 | Case-sensitivity of result | CWE-178 | case affects the resolved file |
| PLAT-096 | Result injection into command | CWE-78 | path used in a shell call downstream |
| PLAT-097 | Result injection into URL | CWE-918 | path used to build a fetch URL |
| PLAT-098 | Info-leak minimization policy | CWE-200 | which fields are safe to expose |
| PLAT-099 | Determinism across runs | CWE-20 | stable, reproducible values |
| PLAT-100 | Fuzz all entry points | CWE-20 | random args/calls never crash |

---

## Additional cases (deeper / documented)

### Information disclosure & fingerprinting

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-101 | Composite host fingerprint | CWE-200 | os+arch+version uniquely identify the build |
| PLAT-102 | Version → CVE lookup | CWE-200 | `hostVersion` reveals patch level for exploit selection |
| PLAT-103 | Arch → payload selection | CWE-200 | `arch` narrows shellcode |
| PLAT-104 | Pointer size → exploit tuning | CWE-200 | 32/64-bit offset selection |
| PLAT-105 | Endianness → exploit tuning | CWE-200 | byte-order for an exploit |
| PLAT-106 | CPU count → infra inference | CWE-200 | scaling/container detail |
| PLAT-107 | Library suffix → OS leak | CWE-200 | `.so`/`.dll`/`.dylib` reveals OS |
| PLAT-108 | Absolute path disclosure | CWE-209 | built path leaks an install dir |
| PLAT-109 | Field echoed to HTTP client | CWE-200 | platform info in a response |
| PLAT-110 | Field written to logs | CWE-532 | platform info logged |
| PLAT-111 | Build metadata in version | CWE-540 | version embeds commit/build host |
| PLAT-112 | Env-derived field leak | CWE-200 | value from a sensitive env var |
| PLAT-113 | WASM/mobile identity leak | CWE-200 | runtime identity over-shared |
| PLAT-114 | Timing-based feature inference | CWE-203 | behavior reveals platform features |
| PLAT-115 | Container vs host CPU leak | CWE-200 | cgroup vs host count discloses topology |

### Path building (`getLibraryPathByName` / `libraryFilename`)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-116 | Traversal in name | CWE-22 | `../../evil` library name |
| PLAT-117 | Traversal in subdir | CWE-22 | `../../` subdir |
| PLAT-118 | Absolute name | CWE-36 | `/lib/evil` |
| PLAT-119 | Absolute subdir override | CWE-22 | absolute subdir ignores the base |
| PLAT-120 | Backslash separators | CWE-22 | `..\` on Windows |
| PLAT-121 | Mixed separators | CWE-22 | `a\b/c` |
| PLAT-122 | NUL in name | CWE-626 | truncation at NUL |
| PLAT-123 | NUL in subdir | CWE-626 | subdir truncation |
| PLAT-124 | Control bytes in name | CWE-74 | control chars in the path |
| PLAT-125 | Empty name/subdir | CWE-20 | empty-argument handling |
| PLAT-126 | Very long name/subdir | CWE-400 | multi-MB argument |
| PLAT-127 | Embedded separator in name | CWE-22 | `/` inside the name |
| PLAT-128 | Overlong UTF-8 in name | CWE-176 | overlong dots |
| PLAT-129 | Trailing dot/space (Windows) | CWE-289 | name normalization |
| PLAT-130 | Result fed to dlopen | CWE-426 | built path enables a library hijack |
| PLAT-131 | Result fed to a shell | CWE-78 | path used in a command downstream |
| PLAT-132 | Result fed to a fetch URL | CWE-918 | path used to build a request |
| PLAT-133 | Filename collision | CWE-706 | two names map to one file |
| PLAT-134 | Extension override via name | CWE-20 | name controls the suffix |
| PLAT-135 | Double-separator result | CWE-20 | `subdir//file` |
| PLAT-136 | Path normalization parity (vs fs) | CWE-697 | platform vs fs normalize differently |
| PLAT-137 | Empty result possibility | CWE-20 | builder returns an empty path |
| PLAT-138 | Non-string args | CWE-20 | number/table arguments |
| PLAT-139 | Path-builder fuzz | CWE-20 | random name/subdir never crash |
| PLAT-140 | Integer overflow in concat size | CWE-190 | length arithmetic on huge args |

### Trust / assumptions / supply chain

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-141 | Security branch on os() | CWE-807 | trusting a spoofable platform value |
| PLAT-142 | Feature gate on version | CWE-693 | version check is spoofable |
| PLAT-143 | Feature detection trusted | CWE-807 | trusting a self-reported flag |
| PLAT-144 | Endianness assumption bug | CWE-188 | wrong branch on endianness |
| PLAT-145 | Pointer-size layout assumption | CWE-188 | struct layout from `pointerSize` |
| PLAT-146 | Path-separator assumption | CWE-20 | hardcoded `/` vs platform |
| PLAT-147 | shlibSuffix mismatch | CWE-20 | wrong extension |
| PLAT-148 | libPrefix mismatch | CWE-20 | wrong `lib` prefix |
| PLAT-149 | Cross-compile value mismatch | CWE-20 | build host vs target |
| PLAT-150 | Version-comparison logic | CWE-697 | string vs semver compare |
| PLAT-151 | Unknown-platform fallback | CWE-20 | graceful unknown handling |
| PLAT-152 | hostVersion injection | CWE-117 | version string with control bytes |
| PLAT-153 | Result trusted without validation | CWE-20 | callers trust the composed path |
| PLAT-154 | Library-name from user input | CWE-22 | request-controlled name |
| PLAT-155 | Subdir from user input | CWE-22 | request-controlled subdir |

### Argument validation & contract

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-156 | Missing required arg | CWE-20 | no-name call |
| PLAT-157 | Extra arguments | CWE-20 | superfluous args |
| PLAT-158 | Wrong arg type | CWE-20 | boolean/function name |
| PLAT-159 | nil argument | CWE-20 | nil name/subdir |
| PLAT-160 | Huge argument | CWE-400 | gigantic strings |
| PLAT-161 | Binary/NUL argument | CWE-626 | length-aware handling |
| PLAT-162 | Invalid UTF-8 argument | CWE-176 | raw bytes |
| PLAT-163 | Locale-affected output | CWE-697 | locale changes formatting |
| PLAT-164 | Return type stability | CWE-20 | documented return types |
| PLAT-165 | os/arch never empty | CWE-20 | non-empty guarantee |
| PLAT-166 | Semver format guarantee | CWE-20 | `x.y.z` shape |
| PLAT-167 | cpuCount ≥ 1 | CWE-20 | never zero/negative |
| PLAT-168 | pointerSize ∈ {4,8} | CWE-20 | only valid values |
| PLAT-169 | endianness ∈ {little,big} | CWE-20 | only valid values |
| PLAT-170 | Stable across calls | CWE-20 | consistent within a run |

### Platform-specific correctness

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-171 | Linux reporting | CWE-20 | identifiers correct |
| PLAT-172 | macOS reporting | CWE-20 | identifiers correct |
| PLAT-173 | Windows reporting | CWE-20 | identifiers correct |
| PLAT-174 | iOS reporting | CWE-20 | mobile identifiers |
| PLAT-175 | Android reporting | CWE-20 | mobile identifiers |
| PLAT-176 | WASM reporting | CWE-20 | `wasm`/`wasm32` correct |
| PLAT-177 | ARM vs x86 arch | CWE-20 | arch string correctness |
| PLAT-178 | Big-endian platform | CWE-20 | endianness correctness |
| PLAT-179 | 32-bit platform | CWE-20 | pointerSize correctness |
| PLAT-180 | Case-sensitivity of result path | CWE-178 | case affects resolution |
| PLAT-181 | Trailing-separator handling | CWE-20 | subdir with a trailing slash |
| PLAT-182 | Non-ASCII library name | CWE-176 | unicode names |
| PLAT-183 | Windows reserved name | CWE-67 | `CON`/`NUL` as a library name |
| PLAT-184 | Short-name (8.3) collision | CWE-66 | Windows short-name aliasing |
| PLAT-185 | Cross-platform separator output | CWE-20 | platform-correct joins |

### Memory / binding / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| PLAT-186 | Concat buffer handling | CWE-120 | path concatenation bounds |
| PLAT-187 | Stack imbalance on error | CWE-664 | error path balanced |
| PLAT-188 | NUL-truncated returns | CWE-626 | returned strings use length |
| PLAT-189 | Exception across boundary | CWE-248 | any throw contained |
| PLAT-190 | Concurrent calls | CWE-362 | thread-safe, no shared mutable global |
| PLAT-191 | Cached-value init race | CWE-362 | value computed once safely |
| PLAT-192 | Reentrant call via metatable | CWE-674 | re-entry during arg coercion |
| PLAT-193 | Memory leak under churn | CWE-401 | repeated calls no leak |
| PLAT-194 | Output buffer reuse race | CWE-664 | shared scratch buffer |
| PLAT-195 | Return ref lifetime | CWE-416 | returned C string lifetime |
| PLAT-196 | Uninitialized field read | CWE-457 | field read before set |
| PLAT-197 | Type confusion in args | CWE-843 | userdata where string expected |
| PLAT-198 | Differential dummy/real | CWE-697 | stub vs real platform diverge |
| PLAT-199 | Determinism across runs | CWE-20 | reproducible values |
| PLAT-200 | Fuzz all entry points | CWE-20 | random args/calls never crash |
