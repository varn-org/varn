# 🧠 core / runtime

The Lua engine, event loop, task pool, runtime lifecycle, native-module registry, and the host entry point.

### Lua sandbox & code execution

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-001 | `os.execute` exposed | CWE-78 | Shell command execution |
| CORE-002 | `io.popen` exposed | CWE-78 | Command execution via `popen` |
| CORE-003 | `io.open` arbitrary file | CWE-73 | Read/write bypassing the `fs` module |
| CORE-004 | `os.getenv` secret read | CWE-200 | Environment secrets exposed |
| CORE-005 | `os.exit` from script | CWE-20 | A script kills the process |
| CORE-006 | `os.remove`/`os.rename` | CWE-73 | Filesystem mutation |
| CORE-007 | `package.loadlib` | CWE-829 | Loading arbitrary native libraries |
| CORE-008 | `require` of native module | CWE-829 | Loading a planted C module |
| CORE-009 | `dofile`/`loadfile` | CWE-73 | Executing arbitrary files |
| CORE-010 | `load`/`loadstring` of text | CWE-95 | Evaluating attacker code |
| CORE-011 | Precompiled bytecode load | CWE-94 | Crafted bytecode subverts the VM |
| CORE-012 | `debug` library exposed | CWE-668 | Introspection/upvalue tampering |
| CORE-013 | `debug.setmetatable` | CWE-913 | Overriding type metatables |
| CORE-014 | `debug.getupvalue/setupvalue` | CWE-913 | Reading/writing closure state |
| CORE-015 | `string.dump` | CWE-200 | Dumping function bytecode |
| CORE-016 | `collectgarbage` abuse | CWE-400 | Forcing GC churn / disabling GC |
| CORE-017 | Global table tampering | CWE-913 | Overwriting `_G`/stdlib functions |
| CORE-018 | Metatable of base types | CWE-913 | Poisoning string/number metatables |
| CORE-019 | `require` path injection | CWE-426 | Manipulation of `package.path`/`cpath` |
| CORE-020 | FFI reachable from script | CWE-829 | Full native power (see `ffi`) |
| CORE-021 | Sandbox bypass via coroutine | CWE-265 | Escaping via coroutine internals |
| CORE-022 | Sandbox bypass via error obj | CWE-265 | Leaking references through errors |
| CORE-023 | `utf8`/`string` edge abuse | CWE-20 | Library edge cases |
| CORE-024 | Bytecode verifier absent | CWE-94 | No verification of loaded chunks |
| CORE-025 | `mode="bt"` accepts bytecode | CWE-94 | The `load` function should restrict to text for untrusted input |

### Resource limits / DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-026 | No instruction cap | CWE-400 | Infinite loop hangs the VM |
| CORE-027 | No memory cap | CWE-400 | A script allocates until OOM |
| CORE-028 | Stack overflow via recursion | CWE-674 | Deep Lua recursion |
| CORE-029 | C-stack overflow via `pcall` depth | CWE-674 | Nested `pcall`/`error` |
| CORE-030 | String interning blowup | CWE-400 | Many unique strings exhaust memory |
| CORE-031 | Table growth blowup | CWE-400 | A giant table |
| CORE-032 | GC pause / stop-the-world | CWE-400 | Huge graphs cause long GC pauses |
| CORE-033 | Event-loop starvation | CWE-834 | Long synchronous work blocks the loop |
| CORE-034 | Task-pool exhaustion | CWE-400 | Many blocking tasks starve the pool |
| CORE-035 | Task-pool deadlock | CWE-833 | A task blocks on a result only the pool can produce |
| CORE-036 | Work-ledger leak | CWE-404 | Retained pending-work keeps the loop alive |
| CORE-037 | Timer queue blowup | CWE-400 | Unbounded timers |
| CORE-038 | Microtask starvation | CWE-834 | Microtasks starve macrotasks |
| CORE-039 | Reentrant loop drive | CWE-674 | Driving the loop from within a callback |
| CORE-040 | Unbounded posted-jobs queue | CWE-400 | A `mainLoop().post` flood |

### Concurrency & lifecycle

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-041 | Teardown UAF (server refs) | CWE-416 | The `lua_State` freed while a server holds refs |
| CORE-042 | Teardown order of modules | CWE-416 | A module freed before its dependents |
| CORE-043 | Cross-thread Lua access | CWE-362 | A worker touches the interpreter |
| CORE-044 | Global state race | CWE-362 | Shared runtime state without a lock |
| CORE-045 | Double-stop runtime | CWE-675 | Calling `stop()` twice |
| CORE-046 | Stop during dispatch | CWE-362 | Stop while a request runs |
| CORE-047 | Signal during GC | CWE-364 | Signal handler runs mid-collection |
| CORE-048 | Worker outlives runtime | CWE-416 | A task references freed runtime state |
| CORE-049 | Promise outlives `lua_State` | CWE-416 | Settle after the state is gone |
| CORE-050 | `atexit`/finalizer ordering | CWE-416 | Static destruction order issues |
| CORE-051 | Thread-pool shutdown join | CWE-833 | Join hangs on a blocked worker |
| CORE-052 | Main-loop wake race | CWE-362 | Wake vs idle-exit predicate |
| CORE-053 | Idle-exit with pending I/O | CWE-662 | Loop exits with sockets/servers active |
| CORE-054 | Re-entrant module install | CWE-674 | Installing modules during install |
| CORE-055 | Registry corruption | CWE-664 | Concurrent registry ref ops |

### Host entry / config / env

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-056 | `arg` injection | CWE-88 | Malicious `argv` interpreted as options |
| CORE-057 | Env var trust (`VARN_*`) | CWE-454 | Reading `VARN_PORT`/TLS paths from a hostile env |
| CORE-058 | `VARN_PORT` overflow | CWE-190 | Huge/negative port value |
| CORE-059 | TLS path from env | CWE-426 | Attacker-controlled cert/key path |
| CORE-060 | Script path traversal | CWE-22 | The script path argument |
| CORE-061 | Working-directory assumption | CWE-22 | Relative paths resolved unexpectedly |
| CORE-062 | Locale-dependent behavior | CWE-697 | Locale changes number/string handling |
| CORE-063 | Unhandled top-level error | CWE-755 | Uncaught error → clean non-zero exit |
| CORE-064 | Error message info leak | CWE-209 | Top-level error reveals internals |
| CORE-065 | Exit-code correctness | CWE-754 | Failure not reflected in the exit code |
| CORE-066 | Stdout/stderr injection | CWE-117 | Control bytes in console output |
| CORE-067 | Huge script file | CWE-400 | Loading a multi-GB script |
| CORE-068 | Script with NUL bytes | CWE-626 | NUL in the source |
| CORE-069 | BOM/shebang handling | CWE-20 | Leading BOM/`#!` line |
| CORE-070 | Chunk-name injection | CWE-117 | Crafted chunk name in errors/logs |

### Native-module registry & bindings

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-071 | Module install order | CWE-696 | Dependency installed after a dependent |
| CORE-072 | Duplicate module registration | CWE-694 | A name registered twice |
| CORE-073 | Module name collision | CWE-694 | Shadowing a built-in |
| CORE-074 | `require` returns cached | CWE-20 | Stale cached module instance |
| CORE-075 | Metatable leak across modules | CWE-913 | A shared metatable mutated |
| CORE-076 | `getRuntime` pointer validity | CWE-825 | Runtime pointer stale in a binding |
| CORE-077 | Light-userdata runtime ptr | CWE-843 | Forged runtime pointer |
| CORE-078 | Binding arg count drift | CWE-20 | Bindings disagree on arity |
| CORE-079 | Shared buffer across bindings | CWE-664 | A reused scratch buffer races |
| CORE-080 | Global function override | CWE-913 | A script replaces a module function |

### Memory safety & fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-081 | Engine OOM handling | CWE-400 | Lua allocator failure handled |
| CORE-082 | Allocator integer overflow | CWE-190 | Growth math overflow |
| CORE-083 | C-API misuse crash | CWE-20 | A binding misuses the C API |
| CORE-084 | `pcall` boundary leak | CWE-401 | Error unwinding leaks resources |
| CORE-085 | `longjmp` over destructors | CWE-755 | A `luaL_error` call skips RAII in C builds |
| CORE-086 | Exception across boundary | CWE-248 | C++ throw reaches the interpreter |
| CORE-087 | Stack overflow detection | CWE-674 | Lua stack limit enforced |
| CORE-088 | Recursive metamethod | CWE-674 | Recursion through `__index`/`__add` |
| CORE-089 | GC finalizer crash | CWE-416 | Errors in `__gc` handled |
| CORE-090 | Weak-table abuse | CWE-401 | Weak references defeat cleanup |
| CORE-091 | Coroutine across states | CWE-362 | Moving a coroutine between states |
| CORE-092 | `xmove` between states | CWE-664 | Unsafe value transfer |
| CORE-093 | Registry index exhaustion | CWE-400 | Endless `luaL_ref` |
| CORE-094 | Upvalue index OOB | CWE-125 | Misuse of `lua_upvalueindex` |
| CORE-095 | Userdata size mismatch | CWE-131 | Wrong size in `lua_newuserdatauv` |
| CORE-096 | Uservalue slot OOB | CWE-125 | Out-of-range uservalue index |
| CORE-097 | Light vs full userdata mix | CWE-843 | Confusion between userdata kinds |
| CORE-098 | Reentrancy in allocator | CWE-674 | Allocation triggers GC re-entry |
| CORE-099 | Time/clock source abuse | CWE-682 | Wall-clock jumps affect the runtime |
| CORE-100 | Fuzz scripts + bindings | CWE-20 | Random scripts/`argv`/env never crash the host |

---

## Additional cases (deeper / documented)

### Lua VM / sandbox (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-101 | Redis Lua sandbox escape | CVE-2022-0543 | A reachable `package`/`luaopen_*` → RCE |
| CORE-102 | `string.rep` OOM | CWE-789 | Calling `("x"):rep(2^40)` exhausts memory |
| CORE-103 | `table.concat`/insert blowup | CWE-400 | Quadratic table building |
| CORE-104 | `string.format` `%` abuse | CWE-134 | Format-spec DoS / type confusion |
| CORE-105 | `string.pack` size overflow | CWE-190 | Pack with huge counts |
| CORE-106 | Pattern matching ReDoS | CWE-1333 | Calling `string.find` with a pathological pattern |
| CORE-107 | `string.gsub` replacement blowup | CWE-400 | Exponential replacement growth |
| CORE-108 | `utf8` library edge | CWE-176 | Malformed sequences |
| CORE-109 | `tonumber` base abuse | CWE-20 | Exotic bases / huge numbers |
| CORE-110 | `select('#', ...)` huge varargs | CWE-400 | Argument flood |
| CORE-111 | Metatable `__index` chain | CWE-674 | Deep/cyclic metatable chain |
| CORE-112 | `__gc` resurrection | CWE-416 | Finalizer revives an object |
| CORE-113 | `__gc` error handling | CWE-755 | Throwing finalizer |
| CORE-114 | Weak-table cleanup defeat | CWE-401 | Strong ref defeats weakness |
| CORE-115 | Coroutine across states | CWE-362 | Moving a coroutine between states |
| CORE-116 | `coroutine.close` misuse | CWE-664 | Closing a running coroutine |
| CORE-117 | Stack-overflow recovery | CWE-674 | Recovery leaves inconsistent state |
| CORE-118 | `pcall` swallows fatal | CWE-390 | Catching a non-recoverable error |
| CORE-119 | `xpcall` handler abuse | CWE-674 | Handler recurses on error |
| CORE-120 | `error` with a table object | CWE-755 | Non-string error propagation |
| CORE-121 | `error` level confusion | CWE-209 | Wrong source attribution |
| CORE-122 | Upvalue sharing leak | CWE-913 | Closures share mutable upvalues |
| CORE-123 | `_ENV` manipulation | CWE-913 | Swapping the environment |
| CORE-124 | Integer/float key aliasing | CWE-704 | Aliasing of `t[1]`/`t[1.0]` |
| CORE-125 | `rawset`/`rawget` bypass | CWE-913 | Bypassing protective metamethods |

### Bytecode / code loading (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-126 | Crafted bytecode load | CWE-94 | Malicious chunk subverts the VM (no verifier) |
| CORE-127 | `load` mode `bt` accepts binary | CWE-94 | Should be text-only for untrusted input |
| CORE-128 | `string.dump` disclosure | CWE-200 | Dumping function bytecode |
| CORE-129 | Truncated bytecode | CWE-20 | Malformed precompiled chunk |
| CORE-130 | Bytecode version mismatch | CWE-20 | Chunk from another Lua version |
| CORE-131 | Endianness in bytecode | CWE-188 | Cross-arch bytecode |
| CORE-132 | `loadstring` of attacker source | CWE-95 | Eval of untrusted Lua |
| CORE-133 | `dofile`/`loadfile` path | CWE-73 | Arbitrary file execution |
| CORE-134 | `require` cpath native load | CWE-829 | Planted C module |
| CORE-135 | Chunk-name injection | CWE-117 | Crafted chunk name in logs/errors |

### Resource limits (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-136 | No instruction-count cap | CWE-400 | Infinite loop hangs the VM |
| CORE-137 | No memory cap (no allocator hook) | CWE-400 | Script allocates to OOM |
| CORE-138 | Allocator hook bypass | CWE-693 | A cap that can be evaded |
| CORE-139 | GC disable via script | CWE-400 | Calling `collectgarbage("stop")` |
| CORE-140 | GC step abuse | CWE-400 | Forcing full collections |
| CORE-141 | String-intern table blowup | CWE-400 | Many unique strings |
| CORE-142 | Deep recursion C-stack | CWE-674 | Native stack overflow |
| CORE-143 | `pcall`-depth C-stack | CWE-674 | Nested `pcall` overflow |
| CORE-144 | Metamethod recursion | CWE-674 | Loops through `__index`/`__add` |
| CORE-145 | Coroutine count blowup | CWE-400 | Unbounded coroutines |
| CORE-146 | Registry index exhaustion | CWE-400 | Endless `luaL_ref` |
| CORE-147 | Userdata growth blowup | CWE-400 | Many userdata objects |

### Event loop / task pool (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-148 | Event-loop starvation | CWE-834 | Long sync work blocks the loop |
| CORE-149 | Task-pool exhaustion | CWE-400 | Blocking tasks starve the pool |
| CORE-150 | Task-pool deadlock | CWE-833 | Task awaits pool-produced result |
| CORE-151 | Posted-jobs queue flood | CWE-400 | Unbounded `post` queue |
| CORE-152 | Work-ledger leak keeps alive | CWE-404 | Loop never exits |
| CORE-153 | Idle-exit with active server | CWE-662 | Premature exit |
| CORE-154 | Wake/idle predicate race | CWE-362 | Wake vs idle-exit |
| CORE-155 | Reentrant loop run | CWE-674 | Calling `run()` within a callback |
| CORE-156 | Timer + I/O fairness | CWE-696 | Starvation between sources |

### Lifecycle / teardown (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-157 | Teardown UAF (server refs) | CWE-416 | The `lua_State` freed with refs held |
| CORE-158 | Module teardown order | CWE-416 | Dependency freed first |
| CORE-159 | Static-init order fiasco | CWE-456 | Global used before init |
| CORE-160 | Static-destruction use | CWE-416 | Global used after destruction |
| CORE-161 | Worker outlives runtime | CWE-416 | Task references freed state |
| CORE-162 | Promise outlives state | CWE-416 | Settle after teardown |
| CORE-163 | Double-stop runtime | CWE-675 | Calling `stop()` twice |
| CORE-164 | Stop during dispatch | CWE-362 | Stop mid-request |
| CORE-165 | Thread-pool join hang | CWE-833 | Join on a blocked worker |
| CORE-166 | Signal during GC | CWE-364 | Handler runs mid-collection |
| CORE-167 | `atexit`/finalizer ordering | CWE-416 | Static dtor order |

### Host entry / env / config (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-168 | `argv` injection | CWE-88 | The `argv` array parsed as options |
| CORE-169 | Env trust (`VARN_*`) | CWE-454 | Hostile env alters config |
| CORE-170 | `VARN_PORT` overflow/negative | CWE-190 | Bad port value |
| CORE-171 | TLS path from env | CWE-426 | Attacker cert/key path |
| CORE-172 | Script path traversal | CWE-22 | The script-path argument |
| CORE-173 | cwd assumption | CWE-22 | Relative paths resolved unexpectedly |
| CORE-174 | Locale-dependent behavior | CWE-697 | Number/case handling by locale |
| CORE-175 | Huge script file | CWE-400 | Multi-GB source |
| CORE-176 | NUL in source | CWE-626 | Source truncation |
| CORE-177 | BOM/shebang handling | CWE-20 | Leading BOM/`#!` |
| CORE-178 | Stdout/stderr control bytes | CWE-117 | Console injection |
| CORE-179 | Exit-code correctness | CWE-754 | Failure not reflected |
| CORE-180 | Top-level error info leak | CWE-209 | Error reveals internals |

### Native registry / bindings / memory (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-181 | Module install order | CWE-696 | Dependency installed late |
| CORE-182 | Duplicate registration | CWE-694 | A name registered twice |
| CORE-183 | Built-in shadowing | CWE-694 | A script replaces a module fn |
| CORE-184 | `getRuntime` pointer stale | CWE-825 | Runtime pointer invalid in a binding |
| CORE-185 | Forged light-userdata runtime ptr | CWE-843 | Fake runtime pointer |
| CORE-186 | Shared scratch buffer race | CWE-664 | Reused buffer across bindings |
| CORE-187 | Userdata size mismatch | CWE-131 | Wrong `lua_newuserdatauv` size |
| CORE-188 | Uservalue slot OOB | CWE-125 | Bad uservalue index |
| CORE-189 | Upvalue index OOB | CWE-125 | Misuse of `lua_upvalueindex` |
| CORE-190 | `xmove` between states | CWE-664 | Unsafe value transfer |
| CORE-191 | Allocator integer overflow | CWE-190 | Growth math overflow |
| CORE-192 | Engine OOM handling | CWE-400 | Allocator failure handled |
| CORE-193 | `longjmp` over C++ dtors | CWE-755 | A `luaL_error` call skips RAII |
| CORE-194 | Exception across boundary | CWE-248 | C++ throw reaches the VM |
| CORE-195 | `pcall` boundary resource leak | CWE-401 | Unwinding leaks resources |
| CORE-196 | Weak-table finalization order | CWE-416 | Finalize order surprise |
| CORE-197 | GC finalizer crash | CWE-416 | Errors in `__gc` |
| CORE-198 | ASan/UBSan trip in core | CWE-125 | Sanitizer finding |
| CORE-199 | Differential dummy/real driver | CWE-697 | Stub vs real diverge on safety |
| CORE-200 | Fuzz scripts + `argv` + env | CWE-20 | Random inputs never crash the host |

---

## Round 3 — deeper / documented

### Lua VM internals (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-201 | Bytecode load without verifier | CWE-94 | Crafted chunk subverts the VM |
| CORE-202 | `string.dump` of a C function | CWE-200 | Bytecode disclosure |
| CORE-203 | Truncated/garbage bytecode | CWE-20 | Malformed precompiled chunk |
| CORE-204 | Cross-version bytecode | CWE-20 | Chunk from another Lua version |
| CORE-205 | Endian/size bytecode mismatch | CWE-188 | Cross-arch chunk |
| CORE-206 | `load` mode allows binary | CWE-94 | Should be text-only for untrusted |
| CORE-207 | Pattern-matching ReDoS | CWE-1333 | A `string.find` pathological pattern |
| CORE-208 | `string.rep` OOM | CWE-789 | Huge repetition |
| CORE-209 | `string.format` `%` abuse | CWE-134 | Format-spec DoS |
| CORE-210 | `string.pack` count overflow | CWE-190 | Huge pack count |
| CORE-211 | `string.gsub` blowup | CWE-400 | Exponential replacement |
| CORE-212 | `table.concat` quadratic | CWE-407 | Quadratic build |
| CORE-213 | `tonumber` base abuse | CWE-20 | Exotic base/huge number |
| CORE-214 | `utf8` library edge | CWE-176 | Malformed sequences |
| CORE-215 | Metatable `__index` cycle | CWE-674 | Cyclic metatable chain |
| CORE-216 | `__gc` resurrection | CWE-416 | Finalizer revives an object |
| CORE-217 | `__gc` throwing | CWE-755 | Finalizer error |
| CORE-218 | Weak-table cleanup defeat | CWE-401 | Strong ref defeats weakness |
| CORE-219 | Coroutine across states | CWE-362 | Moving a coroutine |
| CORE-220 | `coroutine.close` misuse | CWE-664 | Closing a running coroutine |
| CORE-221 | `_ENV` swap | CWE-913 | Environment manipulation |
| CORE-222 | `rawset`/`rawget` bypass | CWE-913 | Bypass protective metamethods |
| CORE-223 | Base-type metatable poison | CWE-913 | String/number metatable |
| CORE-224 | Integer/float key aliasing | CWE-704 | Aliasing of `t[1]` vs `t[1.0]` |
| CORE-225 | Upvalue sharing leak | CWE-913 | Closures share mutable state |

### Sandbox / stdlib exposure (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-226 | `os.execute` reachable | CWE-78 | Shell execution |
| CORE-227 | `io.popen` reachable | CWE-78 | Command execution |
| CORE-228 | `io.open` arbitrary file | CWE-73 | Bypass the `fs` module |
| CORE-229 | `os.getenv` secret read | CWE-200 | Env disclosure |
| CORE-230 | `os.exit` from script | CWE-20 | Script kills the process |
| CORE-231 | `os.remove`/`rename` | CWE-73 | Filesystem mutation |
| CORE-232 | `package.loadlib` | CWE-829 | Load native code |
| CORE-233 | `require` native module | CWE-829 | Planted C module |
| CORE-234 | `dofile`/`loadfile` | CWE-73 | Execute arbitrary files |
| CORE-235 | `load`/`loadstring` text | CWE-95 | Eval untrusted Lua |
| CORE-236 | `debug` library exposed | CWE-668 | Introspection/tampering |
| CORE-237 | `debug.setmetatable` | CWE-913 | Override type metatables |
| CORE-238 | `debug.getupvalue/setupvalue` | CWE-913 | Closure-state access |
| CORE-239 | `collectgarbage` abuse | CWE-400 | GC churn / disable |
| CORE-240 | `package.path`/`cpath` injection | CWE-426 | Search-path manipulation |
| CORE-241 | FFI reachable from script | CWE-829 | Full native power |
| CORE-242 | Sandbox escape via coroutine | CWE-265 | Coroutine internals |
| CORE-243 | Sandbox escape via error obj | CWE-265 | Leak refs through errors |

### Resource limits (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-244 | No instruction-count cap | CWE-400 | Infinite loop hangs the VM |
| CORE-245 | No memory cap (allocator hook) | CWE-400 | Allocate to OOM |
| CORE-246 | Allocator-hook bypass | CWE-693 | Evadable cap |
| CORE-247 | GC stop via script | CWE-400 | Calling `collectgarbage("stop")` |
| CORE-248 | GC step abuse | CWE-400 | Forced full collections |
| CORE-249 | String-intern blowup | CWE-400 | Many unique strings |
| CORE-250 | Deep Lua recursion | CWE-674 | Native stack overflow |
| CORE-251 | Nested `pcall` depth | CWE-674 | C-stack overflow |
| CORE-252 | Metamethod recursion | CWE-674 | A loop through `__index`/`__add` |
| CORE-253 | Coroutine count blowup | CWE-400 | Unbounded coroutines |
| CORE-254 | Registry index exhaustion | CWE-400 | Endless `luaL_ref` |
| CORE-255 | Userdata growth blowup | CWE-400 | Many userdata |

### Event loop / task pool (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-256 | Event-loop starvation | CWE-834 | Long sync work blocks the loop |
| CORE-257 | Task-pool exhaustion | CWE-400 | Blocking tasks starve the pool |
| CORE-258 | Task-pool deadlock | CWE-833 | Task awaits a pool-produced result |
| CORE-259 | Posted-jobs flood | CWE-400 | Unbounded `post` queue |
| CORE-260 | Work-ledger leak | CWE-404 | Loop never exits |
| CORE-261 | Idle-exit with active server | CWE-662 | Premature exit |
| CORE-262 | Wake/idle predicate race | CWE-362 | Wake vs idle-exit |
| CORE-263 | Reentrant loop run | CWE-674 | Calling `run` within a callback |
| CORE-264 | Timer + I/O fairness | CWE-696 | Source starvation |

### Lifecycle / teardown (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-265 | Teardown UAF (server refs) | CWE-416 | The `lua_State` freed with refs held |
| CORE-266 | Module teardown order | CWE-416 | Dependency freed first |
| CORE-267 | Static-init order fiasco | CWE-456 | Global used before init |
| CORE-268 | Static-destruction use | CWE-416 | Global used after destruction |
| CORE-269 | Worker outlives runtime | CWE-416 | Task references freed state |
| CORE-270 | Promise outlives state | CWE-416 | Settle after teardown |
| CORE-271 | Double-stop runtime | CWE-675 | Calling `stop()` twice |
| CORE-272 | Stop during dispatch | CWE-362 | Stop mid-request |
| CORE-273 | Thread-pool join hang | CWE-833 | Join on a blocked worker |
| CORE-274 | Signal during GC | CWE-364 | Handler mid-collection |
| CORE-275 | `atexit`/finalizer ordering | CWE-416 | Static dtor order |

### Host entry / env / registry / memory (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-276 | `argv` injection | CWE-88 | The `argv` array parsed as options |
| CORE-277 | Env trust (`VARN_*`) | CWE-454 | Hostile env alters config |
| CORE-278 | `VARN_PORT` overflow/negative | CWE-190 | Bad port value |
| CORE-279 | TLS path from env | CWE-426 | Attacker cert/key path |
| CORE-280 | Script path traversal | CWE-22 | Script-path argument |
| CORE-281 | cwd assumption | CWE-22 | Relative path resolution |
| CORE-282 | Locale-dependent behavior | CWE-697 | Number/case handling |
| CORE-283 | Huge script file | CWE-400 | Multi-GB source |
| CORE-284 | NUL in source | CWE-626 | Source truncation |
| CORE-285 | BOM/shebang handling | CWE-20 | Leading BOM/`#!` |
| CORE-286 | Stdout/stderr control bytes | CWE-117 | Console injection |
| CORE-287 | Exit-code correctness | CWE-754 | Failure not reflected |
| CORE-288 | Top-level error info leak | CWE-209 | Error reveals internals |
| CORE-289 | Duplicate module registration | CWE-694 | A name registered twice |
| CORE-290 | Built-in shadowing | CWE-694 | A script replaces a module fn |
| CORE-291 | `getRuntime` pointer stale | CWE-825 | Invalid runtime pointer |
| CORE-292 | Forged light-userdata runtime ptr | CWE-843 | Fake runtime pointer |
| CORE-293 | Userdata size mismatch | CWE-131 | Wrong allocation size |
| CORE-294 | Uservalue/upvalue index OOB | CWE-125 | Bad index |
| CORE-295 | `xmove` between states | CWE-664 | Unsafe value transfer |
| CORE-296 | Allocator integer overflow | CWE-190 | Growth math overflow |
| CORE-297 | `longjmp` over C++ dtors | CWE-755 | A `luaL_error` call skips RAII |
| CORE-298 | Exception across boundary | CWE-248 | C++ throw reaches the VM |
| CORE-299 | ASan/UBSan trip in core | CWE-125 | Sanitizer finding |
| CORE-300 | Fuzz scripts + `argv` + env | CWE-20 | Random inputs never crash the host |
