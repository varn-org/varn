# Cross-cutting tests (apply to every module)

Combine each axis (memory, integer, concurrency, resource/OOM, input, error handling) with every entry point of every module. These rows are the shared baseline.

### Memory safety (C/C++)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-001 | Stack buffer overflow | CWE-121 | Oversized input into a fixed stack buffer overruns the frame / return address |
| XC-002 | Heap buffer overflow | CWE-122 | Length/offset drives a write past a heap allocation |
| XC-003 | Global/static overflow | CWE-120 | Write past a static buffer corrupts adjacent globals |
| XC-004 | Out-of-bounds read | CWE-125 | Crafted offset/length reads past a buffer, leaking memory |
| XC-005 | Out-of-bounds write | CWE-787 | Controlled index writes outside a buffer → corruption/RCE primitive |
| XC-006 | Off-by-one | CWE-193 | Boundary index `== len` writes/reads one element too far |
| XC-007 | Use-after-free | CWE-416 | Object used after free/GC. Reclaimed memory is attacker-controlled |
| XC-008 | Use-after-return | CWE-562 | A pointer to a stack local escapes its scope |
| XC-009 | Use-after-move | CWE-825 | A moved-from C++ object is read |
| XC-010 | Double free | CWE-415 | Freeing the same pointer twice corrupts allocator metadata |
| XC-011 | Invalid free | CWE-590 | Freeing a non-heap or mid-block pointer |
| XC-012 | Mismatched `new`/`delete` | CWE-762 | Memory from `new[]` freed with `delete` (or `malloc`/`delete`) |
| XC-013 | Null pointer dereference | CWE-476 | An unchecked null return is dereferenced |
| XC-014 | Wild/dangling pointer | CWE-824 | Dereferencing an uninitialized or stale pointer |
| XC-015 | Uninitialized memory use | CWE-457 | Reading a field/var before assignment leaks stale data |
| XC-016 | Type confusion (userdata) | CWE-843 | Wrong userdata metatable reinterprets memory |
| XC-017 | Type confusion (downcast) | CWE-843 | Unchecked `static_cast` between unrelated types |
| XC-018 | Memory leak | CWE-401 | An allocation path never frees under churn/errors |
| XC-019 | Iterator/reference invalidation | CWE-416 | Mutating a container while holding an iterator/reference into it |
| XC-020 | Format string | CWE-134 | User data flows into a printf-style format |
| XC-021 | `memcpy`/`strcpy` unbounded | CWE-120 | Length taken from input, not the destination size |
| XC-022 | `alloca`/VLA on input size | CWE-789 | Stack allocation sized by attacker input |
| XC-023 | Unchecked `realloc` | CWE-252 | A `realloc` failure leaks/overwrites the original pointer |
| XC-024 | Return value of alloc unchecked | CWE-252/690 | Null allocation dereferenced |
| XC-025 | Buffer under-read/write | CWE-124 | Negative offset writes/reads before the buffer |

### Integer & arithmetic

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-026 | Unsigned wraparound | CWE-190 | The sum `size + n` wraps to a tiny allocation, then overflows |
| XC-027 | Signed overflow (UB) | CWE-190 | Signed arithmetic overflow is undefined |
| XC-028 | Integer underflow | CWE-191 | The difference `len - k` underflows to a huge unsigned size |
| XC-029 | `size_t`→`int` truncation | CWE-197 | Length > `INT_MAX` truncates negative, defeating checks |
| XC-030 | Sign-extension bug | CWE-194 | A negative `int` length becomes huge as `size_t` |
| XC-031 | Multiplication overflow on alloc | CWE-190 | The product `count * size` overflows the allocation size |
| XC-032 | Off-by-one in length math | CWE-193 | Stray `+1`/`-1` errors in size computations |
| XC-033 | Lossy float↔int | CWE-681 | A `lua_Number`→`int` conversion loses/wraps large values |
| XC-034 | Modulo/division by zero | CWE-369 | Attacker drives a zero divisor |
| XC-035 | Pointer arithmetic overflow | CWE-468 | The expression `ptr + n` overflows or escapes the object |

### Lua binding & runtime

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-036 | Stack imbalance on success | CWE-664 | A binding leaves extra values / wrong return count |
| XC-037 | Stack imbalance on error path | CWE-664 | An early `return`/`luaL_error` leaks stack values |
| XC-038 | `lua_checkstack` omitted | CWE-674 | Deep pushes overflow the Lua stack |
| XC-039 | Registry ref leak | CWE-401 | A `luaL_ref` never released under churn |
| XC-040 | Registry double-unref | CWE-672 | A freed registry slot is reused |
| XC-041 | Missing argument type check | CWE-20 | Wrong/missing/nil args reach C assuming types |
| XC-042 | Wrong `luaL_checkudata` type | CWE-843 | A foreign userdata passes a metatable check |
| XC-043 | `lua_tostring` on non-string coercion | CWE-704 | Implicit number→string coercion mutates a table key mid-iteration |
| XC-044 | NUL-truncated string | CWE-626 | Using `lua_tostring` (no length) truncates binary data at `\0` |
| XC-045 | C++ exception across C boundary | CWE-248 | A `throw` crosses a `lua_CFunction` |
| XC-046 | `longjmp` over C++ destructors | CWE-755 | A `luaL_error` skips RAII cleanup in non-C++ Lua builds |
| XC-047 | Unprotected callback error | CWE-755 | A hook/handler error tears down the server |
| XC-048 | Re-entrant binding | CWE-674 | A binding invoked re-entrantly via a metamethod corrupts its state |
| XC-049 | `__gc` resurrection | CWE-416 | A finalizer revives or double-frees an object |
| XC-050 | Metatable override of internals | CWE-913 | Overriding `__index`/`__gc` on a managed userdata |
| XC-051 | Light-userdata confusion | CWE-843 | A forged light userdata pointer is dereferenced |
| XC-052 | Coroutine misuse | CWE-662 | Yielding across a C boundary that cannot resume |
| XC-053 | Upvalue tampering | CWE-913 | Manipulating closure upvalues used as trusted state |
| XC-054 | Integer/float key aliasing | CWE-704 | Key confusion between `t[1]` and `t[1.0]` in table access |
| XC-055 | Huge return/arg count | CWE-400 | A binding pushing millions of values exhausts memory |

### Concurrency

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-056 | Cross-thread Lua access | CWE-362 | Touching `lua_State` off the main loop races the interpreter |
| XC-057 | Data race on shared map | CWE-362 | Concurrent unguarded access corrupts a container |
| XC-058 | Lost update | CWE-362 | Two threads read-modify-write the same counter |
| XC-059 | TOCTOU | CWE-367 | State changes between a check and the dependent use |
| XC-060 | Deadlock (lock order) | CWE-833 | Two locks acquired in opposite orders |
| XC-061 | Deadlock on shutdown join | CWE-833 | A worker blocks on the main loop while it is joined |
| XC-062 | Condition-variable lost wakeup | CWE-662 | A notify races a wait without the predicate re-check |
| XC-063 | Reentrant callback deadlock | CWE-833 | A callback re-enters code holding the same lock |
| XC-064 | Atomicity violation | CWE-366 | A multi-step invariant observed mid-update |
| XC-065 | Memory-ordering bug | CWE-1264 | Missing acquire/release lets a stale value be read |
| XC-066 | Thread/promise leak | CWE-772 | Abandoned coroutines/threads accumulate |
| XC-067 | Signal/async-unsafe handler | CWE-364 | Non-async-signal-safe work in a handler |
| XC-068 | Double-resume of coroutine | CWE-362 | Resuming an already-running/finished coroutine |
| XC-069 | Shared RNG/state race | CWE-362 | Concurrent use of a non-thread-safe global |
| XC-070 | Order-dependent finalization | CWE-416 | Global teardown frees a dependency still in use |

### Resource exhaustion / OOM / DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-071 | Unbounded string input | CWE-400 | A multi-GB string exhausts memory |
| XC-072 | Unbounded count argument | CWE-789 | A huge count drives a giant allocation |
| XC-073 | Deep recursion | CWE-674 | Deeply nested input overflows the native stack |
| XC-074 | Algorithmic complexity | CWE-407 | Worst-case input drives super-linear CPU |
| XC-075 | Hash-collision flood | CWE-407 | Colliding keys degrade a hash map to O(n) |
| XC-076 | Quadratic string building | CWE-407 | Repeated concat/`realloc` on growing input |
| XC-077 | Unbounded recursion in GC | CWE-674 | A deeply linked object graph overflows on collection |
| XC-078 | FD/handle leak | CWE-404 | Objects/connections never closed exhaust descriptors |
| XC-079 | Thread/worker exhaustion | CWE-400 | Many blocking tasks starve the pool |
| XC-080 | Memory amplification | CWE-409 | Small input expands to huge in-memory output |
| XC-081 | Allocation storm | CWE-400 | Many tiny allocations fragment/exhaust the heap |
| XC-082 | Busy-spin starvation | CWE-834 | A tight loop starves the event loop |

### Input hygiene & encoding

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-083 | Empty input | CWE-20 | Empty string/array/table hits an unhandled edge |
| XC-084 | Embedded NUL | CWE-626 | A `\0` mid-string truncates or splits |
| XC-085 | Control bytes | CWE-74 | Bytes `\x00`-`\x1f`/`\x7f` desync parsing/logging |
| XC-086 | Invalid UTF-8 | CWE-176 | Malformed byte sequences crash or mis-decode |
| XC-087 | Overlong UTF-8 | CWE-176 | Overlong encodings bypass byte filters |
| XC-088 | Unicode normalization | CWE-178 | NFC/NFD/confusables defeat string comparisons |
| XC-089 | Max/min numeric | CWE-20 | Edge values `MAX_INT`, `MIN_INT`, `inf`, `nan` |
| XC-090 | Negative size/index | CWE-20 | Negative lengths/offsets bypass checks |
| XC-091 | Type-mismatch argument | CWE-20 | Table where string expected, etc. |
| XC-092 | Huge nesting depth | CWE-674 | Nesting beyond parser/recursion limits |
| XC-093 | Trailing/garbage bytes | CWE-20 | Extra bytes after a valid structure |
| XC-094 | Locale-dependent parsing | CWE-697 | Decimal `,` vs `.`, case folding by locale |
| XC-095 | BOM / leading whitespace | CWE-20 | A byte-order mark or padding changes parsing |
| XC-096 | Mixed line endings | CWE-93 | Inconsistencies between `\r`, `\n`, `\r\n` |

### Error handling & disclosure

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-097 | Internal detail leak | CWE-209 | Stack traces / internal paths returned to the caller |
| XC-098 | Secret in logs/errors | CWE-532 | Tokens/keys/session ids written to logs |
| XC-099 | Fail-open on error | CWE-636 | An error during a security check allows instead of denies |
| XC-100 | Silent truncation | CWE-252 | A dropped/ignored error yields partial, trusted-looking output |
| XC-101 | Inconsistent error channels | CWE-755 | Mixing `nil`+`err` vs throw vs return code, masking failures |
| XC-102 | Resource not released on error | CWE-404 | An exception path leaks the handle it opened |


---

## Additional cases (deeper / rarer)

### Compiler, UB & optimizer

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-103 | Optimized-away `memset` | CWE-14 | Dead-store elimination keeps secrets in memory |
| XC-104 | Strict-aliasing violation | CWE-758 | Type-punning UB miscompiles a check |
| XC-105 | Signed-overflow assumed-impossible | CWE-190 | Optimizer removes an overflow check |
| XC-106 | Null-check removed after deref | CWE-476 | UB lets the compiler drop a null guard |
| XC-107 | Unsequenced modification | CWE-758 | UB in the style of `i = i++` |
| XC-108 | Uninitialized read as UB | CWE-457 | Optimizer assumes a value |
| XC-109 | Out-of-bounds assumed UB | CWE-125 | Bounds check elided |
| XC-110 | `restrict`/aliasing contract break | CWE-758 | Overlapping buffers passed to a `restrict` API |
| XC-111 | Volatile/atomic misuse | CWE-1264 | A flag not atomic across threads |
| XC-112 | Padding leak in struct copy | CWE-200 | Uninitialized padding sent/serialized |
| XC-113 | Enum out-of-range value | CWE-704 | A switch misses an unexpected enum |
| XC-114 | Float exactness assumption | CWE-1077 | Using `==` on floats branches wrongly |
| XC-115 | `-ffast-math` NaN handling | CWE-697 | Fast-math breaks NaN comparisons |
| XC-116 | Inline-asm/intrinsic misuse | CWE-758 | Hand-tuned code violates ABI |

### Sanitizer / dynamic-analysis findings

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-117 | ASan heap-overflow trip | CWE-122 | Red-zone write detected under ASan |
| XC-118 | ASan stack-overflow trip | CWE-121 | Stack red-zone write |
| XC-119 | ASan use-after-free trip | CWE-416 | Quarantined-memory access |
| XC-120 | ASan double-free trip | CWE-415 | Detected double free |
| XC-121 | LeakSanitizer report | CWE-401 | Leaks at exit |
| XC-122 | MSan uninitialized read | CWE-457 | MemorySanitizer trip |
| XC-123 | UBSan signed overflow | CWE-190 | UBSan trap |
| XC-124 | UBSan misaligned access | CWE-1257 | Unaligned load/store |
| XC-125 | UBSan null deref | CWE-476 | UBSan null trap |
| XC-126 | TSan data race | CWE-362 | ThreadSanitizer report |
| XC-127 | TSan lock-order inversion | CWE-833 | Potential deadlock flagged |
| XC-128 | Valgrind invalid read/write | CWE-125/787 | A memcheck error |
| XC-129 | Coverage-guided fuzz crash | CWE-20 | A libFuzzer/AFL finding |
| XC-130 | Differential fuzz divergence | CWE-697 | Two paths disagree on the same input |

### Side-channels & oracles

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-131 | Timing oracle on compare | CWE-208 | Branch timing leaks a secret |
| XC-132 | Error-message oracle | CWE-203 | Distinct errors reveal internal state |
| XC-133 | Response-size oracle | CWE-204 | Output length leaks a secret |
| XC-134 | Cache-timing side-channel | CWE-385 | Data-dependent memory access |
| XC-135 | Branch-prediction leak | CWE-1300 | Secret-dependent branch |
| XC-136 | Early-exit on first mismatch | CWE-208 | Loop returns on first differing byte |
| XC-137 | Allocation-pattern leak | CWE-200 | Alloc size reveals input size |
| XC-138 | GC-timing observable | CWE-385 | Collection pauses leak workload |
| XC-139 | Exception-vs-return timing | CWE-208 | Throw path slower than success |
| XC-140 | Spectre-class gadget | CWE-1303 | Speculative bounds bypass |

### Supply chain & build

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-141 | Vulnerable dependency | CWE-1395 | A bundled lib has a known CVE |
| XC-142 | Transitive dep CVE | CWE-1395 | Indirect dependency vulnerability |
| XC-143 | Unpinned dependency version | CWE-494 | Non-reproducible/poisonable fetch |
| XC-144 | Missing checksum on fetch | CWE-494 | CPM download not integrity-checked |
| XC-145 | Stack protector disabled | CWE-693 | No `-fstack-protector` |
| XC-146 | RELRO/PIE/NX missing | CWE-1327 | Weak binary hardening |
| XC-147 | Fortify-source off | CWE-693 | The `_FORTIFY_SOURCE` macro not set |
| XC-148 | CFI/safe-stack absent | CWE-1325 | No control-flow integrity |
| XC-149 | Debug symbols/info leak | CWE-200 | Shipped symbols reveal internals |
| XC-150 | Assertions disabled in release | CWE-617 | Defining `NDEBUG` drops a security assert |
| XC-151 | Build-time secret embedded | CWE-540 | Secret compiled into the binary |
| XC-152 | Reproducible-build drift | CWE-494 | Non-deterministic artifacts |

### Locale / i18n / encoding (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-153 | Turkish-I case folding | CWE-178 | The `I`/`ı` pair breaks case-insensitive checks |
| XC-154 | Unicode confusables | CWE-1007 | Homoglyph spoofing |
| XC-155 | Bidi override (Trojan Source) | CWE-1007 | RLO/LRO reorders text/code |
| XC-156 | Zero-width characters | CWE-176 | ZWSP/ZWJ defeat filters |
| XC-157 | Combining-character flood | CWE-400 | Zalgo text expansion |
| XC-158 | Normalization-form mismatch | CWE-178 | NFC vs NFD comparison bypass |
| XC-159 | Case-folding expansion | CWE-178 | A `ß`→`ss` length change |
| XC-160 | Locale decimal separator | CWE-697 | Decimal `,` vs `.` parsing |
| XC-161 | Locale collation order | CWE-697 | Sort/compare differs by locale |
| XC-162 | Surrogate/WTF-8 handling | CWE-176 | Lone surrogates round-trip |

### Concurrency (exotic)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-163 | ABA problem | CWE-362 | A value cycles back, defeating a CAS |
| XC-164 | Priority inversion | CWE-833 | Low-priority holder blocks high-priority |
| XC-165 | False sharing perf cliff | CWE-1037 | Cache-line contention DoS |
| XC-166 | Double-checked locking | CWE-609 | Unsafe lazy init |
| XC-167 | Thundering herd | CWE-400 | Many threads wake on one event |
| XC-168 | Livelock | CWE-833 | Threads spin without progress |
| XC-169 | Memory reclamation race | CWE-416 | Freeing while a reader holds it |
| XC-170 | Signal vs lock | CWE-364 | A signal handler grabs a held lock |
| XC-171 | `fork()` with locks held | CWE-662 | Child inherits a locked mutex |
| XC-172 | TLS (thread-local) leak | CWE-401 | Thread-local state never released |

### Resource & lifecycle (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-173 | Static-init order fiasco | CWE-456 | Global used before construction |
| XC-174 | Static-destruction use | CWE-416 | Global used after destruction |
| XC-175 | Exception during static init | CWE-755 | A throw before `main` |
| XC-176 | Recursion via destructor | CWE-674 | Dtor triggers more frees |
| XC-177 | Resource limit (`ulimit`) hit | CWE-400 | Behavior on `RLIMIT` exhaustion |
| XC-178 | Memory fragmentation DoS | CWE-400 | Allocator fragmentation |
| XC-179 | Page-cache thrash | CWE-400 | Working set exceeds RAM |
| XC-180 | Handle inheritance to child | CWE-403 | FDs leaked across `exec` |
| XC-181 | Temp-file cleanup miss | CWE-459 | Temp files left on error |
| XC-182 | Lock not released on throw | CWE-667 | Exception skips unlock |

### Error handling & state (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-183 | Error code ignored | CWE-252 | Return value not checked |
| XC-184 | Partial-failure mid-batch | CWE-459 | Half-applied operation |
| XC-185 | Inconsistent state after error | CWE-755 | Invariant broken on rollback |
| XC-186 | Retry without idempotency | CWE-696 | Retried op duplicates effects |
| XC-187 | Exception in destructor | CWE-248 | Throwing dtor terminates |
| XC-188 | swallowed exception | CWE-390 | Catch-all hides a real failure |
| XC-189 | Error during error handling | CWE-755 | Secondary failure masks the first |
| XC-190 | `NULL` vs empty confusion | CWE-476 | Absent vs empty value mixed |
| XC-191 | Boolean-coercion surprise | CWE-697 | Truthiness bug with `0`/`""`/`nil` |
| XC-192 | Default-deny not enforced | CWE-636 | Missing case defaults to allow |

### Observability, supply, misc

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-193 | No audit trail of security events | CWE-778 | Attacks leave no trace |
| XC-194 | Excessive logging hides signal | CWE-779 | Noise buries the real event |
| XC-195 | Metrics leak internal state | CWE-200 | Counters reveal secrets/sizes |
| XC-196 | Crash dump leaks memory | CWE-528 | Core dump exposes secrets |
| XC-197 | Predictable resource IDs | CWE-340 | Guessable handles/ids |
| XC-198 | Insecure default config | CWE-1188 | Unsafe out-of-the-box settings |
| XC-199 | Feature flag bypass | CWE-693 | A debug flag reaches production |
| XC-200 | Backwards-compat shim risk | CWE-477 | Legacy path re-enables a flaw |
| XC-201 | Clock dependency for security | CWE-367 | Logic trusts wall-clock time |
| XC-202 | Differential behavior across builds | CWE-697 | Dummy vs real driver disagree on safety |

---

## Round 3 — deeper / documented

### Exploit mitigations & binary hardening

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-203 | ASLR not enabled (no PIE) | CWE-1327 | Predictable addresses ease exploitation |
| XC-204 | W^X / writable+executable page | CWE-1303 | RWX memory permits code injection |
| XC-205 | Missing stack canary | CWE-693 | Stack overflow not detected |
| XC-206 | GOT not read-only (no RELRO) | CWE-1327 | GOT overwrite hijacks calls |
| XC-207 | No CFI / forward-edge | CWE-1325 | Indirect-call hijack |
| XC-208 | No shadow stack / backward-edge | CWE-1322 | ROP via return addresses |
| XC-209 | Guard pages absent | CWE-789 | Stack/heap overrun not trapped |
| XC-210 | Heap metadata unprotected | CWE-122 | Classic heap grooming |
| XC-211 | Predictable allocator | CWE-340 | Deterministic heap layout |
| XC-212 | Freed memory not wiped | CWE-226 | Freed secrets recoverable |
| XC-213 | Executable stack | CWE-1327 | NX disabled |
| XC-214 | Missing FORTIFY checks | CWE-120 | Unchecked libc string ops |

### Concurrency (models & primitives)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-215 | Sequential-consistency assumption | CWE-1264 | Relaxed atomics reorder |
| XC-216 | Non-atomic 64-bit on 32-bit | CWE-366 | Torn 64-bit read/write |
| XC-217 | Spinlock under contention | CWE-1037 | CPU burn and priority issues |
| XC-218 | RW-lock writer starvation | CWE-410 | Readers starve a writer |
| XC-219 | Recursive-lock assumption | CWE-667 | Non-recursive mutex re-locked |
| XC-220 | Condition predicate not re-checked | CWE-662 | Wakeup without re-test |
| XC-221 | Atomic ref-count underflow | CWE-191 | Premature free |
| XC-222 | `shared_ptr` control-block race | CWE-362 | Concurrent copy/reset |
| XC-223 | `weak_ptr` lock race | CWE-416 | A `lock()` after expiry |
| XC-224 | Callback during teardown | CWE-416 | Event fires on a dying object |
| XC-225 | Two-phase init exposed | CWE-665 | Object used before fully built |
| XC-226 | Re-entrancy via observer | CWE-674 | Observer mutates the subject |

### Numeric / encoding (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-227 | `INT_MIN` negation UB | CWE-190 | Negating the minimum overflows |
| XC-228 | Division `INT_MIN / -1` | CWE-369 | Overflow trap |
| XC-229 | Shift by >= width | CWE-1335 | UB shift amount |
| XC-230 | Implicit narrowing in init | CWE-197 | Brace-init narrowing |
| XC-231 | `char` signedness portability | CWE-697 | Signed vs unsigned `char` |
| XC-232 | size mixing `ptrdiff_t`/`size_t` | CWE-195 | Signed/unsigned compare |
| XC-233 | float-to-int out of range | CWE-681 | UB conversion |
| XC-234 | NaN-aware comparator | CWE-697 | NaN breaks strict-weak ordering |
| XC-235 | Endian-dependent serialization | CWE-188 | Wire format differs by host |
| XC-236 | UTF-8 BOM mid-stream | CWE-176 | BOM treated as data |
| XC-237 | CESU-8 / Modified UTF-8 | CWE-176 | Non-standard encodings |
| XC-238 | Unicode case-fold roundtrip | CWE-178 | Fold then compare mismatch |

### Side-channels & oracles (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-239 | Microarchitectural data sampling | CWE-1303 | MDS-class leak |
| XC-240 | Port-contention timing | CWE-1300 | SMT side-channel |
| XC-241 | Memory-deduplication leak | CWE-204 | Page-dedup oracle |
| XC-242 | Compression-ratio oracle | CWE-204 | Size leaks secret content |
| XC-243 | Padding-length oracle | CWE-203 | Distinct padding errors |
| XC-244 | Timing exfil channel | CWE-385 | Covert timing channel |
| XC-245 | Error-code enumeration | CWE-203 | Error differences map state |
| XC-246 | Lazy-allocation timing | CWE-385 | First-touch page timing |

### Supply chain & build provenance

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-247 | Dependency confusion | CWE-427 | A public package shadows an internal one |
| XC-248 | Typosquatted dependency | CWE-1357 | Near-name malicious package |
| XC-249 | Compromised upstream tag | CWE-494 | Moved/retagged release |
| XC-250 | Missing SBOM | CWE-1104 | Unknown component inventory |
| XC-251 | Unsigned artifacts | CWE-347 | No release signature |
| XC-252 | Toolchain backdoor | CWE-506 | Compromised compiler |
| XC-253 | Vendored-lib drift | CWE-1395 | Bundled copy lags upstream fixes |
| XC-254 | Patch not applied to vendor copy | CWE-1395 | Local fork misses a CVE fix |
| XC-255 | CI secret exposure | CWE-522 | Build-time credential leak |
| XC-256 | Build cache poisoning | CWE-349 | Tainted cached artifact |

### Error handling / state (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-257 | Error swallowed by destructor | CWE-460 | Cleanup hides the failure |
| XC-258 | Inconsistent partial commit | CWE-662 | No all-or-nothing |
| XC-259 | Compensating-action failure | CWE-755 | Rollback itself fails |
| XC-260 | Re-entrant error handler | CWE-674 | Handler triggers the same error |
| XC-261 | Error-path double-release | CWE-415 | Cleanup runs on both paths |
| XC-262 | Silent default on unknown enum | CWE-478 | Missing case falls through |
| XC-263 | Assertion as control flow | CWE-617 | Assert disabled in release changes flow |
| XC-264 | Status vs exception mix | CWE-755 | Inconsistent error reporting |
| XC-265 | Truncated-write not detected | CWE-393 | Short write treated as success |
| XC-266 | Negative-result coercion | CWE-697 | A `-1` error code used as a size |

### Observability / audit / config

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-267 | No tamper-evident audit log | CWE-778 | Attacker edits the trail |
| XC-268 | Metrics leak cardinality | CWE-200 | Label values reveal data |
| XC-269 | Health endpoint info leak | CWE-200 | Versions/paths in a health route |
| XC-270 | Debug build in production | CWE-489 | Debug features shipped |
| XC-271 | Verbose default logging | CWE-1295 | Secrets in default logs |
| XC-272 | Insecure default bind/port | CWE-1188 | Public by default |
| XC-273 | Hardening off by default | CWE-1188 | Security feature disabled out of the box |
| XC-274 | Config injection via env | CWE-454 | Env overrides a security setting |
| XC-275 | Feature-flag persistence bug | CWE-693 | A temporary flag sticks |
| XC-276 | Config reload TOCTOU | CWE-367 | Config changes mid-request |

### Memory safety (deeper CWEs)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-277 | Container OOB via bad iterator | CWE-125 | Dereferencing `end()`/an invalid iterator |
| XC-278 | `string_view` dangling | CWE-416 | View outlives its buffer |
| XC-279 | `span` over a temporary | CWE-416 | Span of a moved/temporary object |
| XC-280 | `optional`/`variant` wrong access | CWE-758 | Reading the wrong alternative |
| XC-281 | Self-move | CWE-665 | Assigning `x = std::move(x)` |
| XC-282 | Vector reallocation invalidation | CWE-416 | Pointer into a grown vector |
| XC-283 | Map node-handle misuse | CWE-416 | Extracted node used after erase |
| XC-284 | `realloc` shrink data loss | CWE-131 | Shrink loses needed bytes |
| XC-285 | Placement-new misalignment | CWE-1257 | Misaligned in-place construct |
| XC-286 | `reinterpret_cast` aliasing | CWE-843 | Invalid type reinterpret |
| XC-287 | Bit-field overflow | CWE-190 | Value exceeds the bit-field width |
| XC-288 | Flexible-array sizing | CWE-131 | Wrong allocation for a trailing array |

### Fuzzing & verification harnesses

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-289 | Coverage-guided fuzz (libFuzzer) | CWE-20 | Per-parser fuzz target |
| XC-290 | Mutation fuzz (AFL++) | CWE-20 | Seed-corpus mutation |
| XC-291 | Grammar-aware fuzz | CWE-20 | Format-aware generation |
| XC-292 | Differential fuzz vs reference | CWE-697 | Compare against a known-good impl |
| XC-293 | Property-based testing | CWE-20 | Invariants over random inputs |
| XC-294 | Stateful/sequence fuzz | CWE-20 | Randomized API call sequences |
| XC-295 | Concurrency fuzz (TSan + stress) | CWE-362 | Race detection under load |
| XC-296 | ASan + fuzz integration | CWE-125 | Memory bugs surfaced by fuzzing |
| XC-297 | Continuous fuzzing corpus | CWE-20 | Regression corpus retained |
| XC-298 | Crash-triage minimization | CWE-20 | Reproduce with a minimal case |

### Time / environment

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XC-299 | Wall-clock used for security | CWE-367 | TTL/expiry uses a settable clock |
| XC-300 | Leap-second / DST edge | CWE-682 | Time math near a discontinuity |
| XC-301 | 32-bit `time_t` (Y2038) | CWE-190 | Timestamp overflow |
| XC-302 | Locale/timezone-dependent parse | CWE-697 | Environment changes interpretation |
