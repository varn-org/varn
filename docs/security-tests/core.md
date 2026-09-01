# 🧠 core / runtime

The Lua engine, event loop, task pool, runtime lifecycle, native-module registry, and the
host entry point.

### Lua sandbox & code execution

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-001 | `os.execute` exposed | CWE-78 | shell command execution |
| CORE-002 | `io.popen` exposed | CWE-78 | command execution via popen |
| CORE-003 | `io.open` arbitrary file | CWE-73 | read/write bypassing the fs module |
| CORE-004 | `os.getenv` secret read | CWE-200 | environment secrets exposed |
| CORE-005 | `os.exit` from script | CWE-20 | a script kills the process |
| CORE-006 | `os.remove`/`os.rename` | CWE-73 | filesystem mutation |
| CORE-007 | `package.loadlib` | CWE-829 | loading arbitrary native libraries |
| CORE-008 | `require` of native module | CWE-829 | loading a planted C module |
| CORE-009 | `dofile`/`loadfile` | CWE-73 | executing arbitrary files |
| CORE-010 | `load`/`loadstring` of text | CWE-95 | evaluating attacker code |
| CORE-011 | Precompiled bytecode load | CWE-94 | crafted bytecode subverts the VM |
| CORE-012 | `debug` library exposed | CWE-668 | introspection/upvalue tampering |
| CORE-013 | `debug.setmetatable` | CWE-913 | overriding type metatables |
| CORE-014 | `debug.getupvalue/setupvalue` | CWE-913 | reading/writing closure state |
| CORE-015 | `string.dump` | CWE-200 | dumping function bytecode |
| CORE-016 | `collectgarbage` abuse | CWE-400 | forcing GC churn / disabling GC |
| CORE-017 | Global table tampering | CWE-913 | overwriting `_G`/stdlib functions |
| CORE-018 | Metatable of base types | CWE-913 | poisoning string/number metatables |
| CORE-019 | `require` path injection | CWE-426 | `package.path`/`cpath` manipulation |
| CORE-020 | FFI reachable from script | CWE-829 | full native power (see ffi) |
| CORE-021 | Sandbox bypass via coroutine | CWE-265 | escaping via coroutine internals |
| CORE-022 | Sandbox bypass via error obj | CWE-265 | leaking references through errors |
| CORE-023 | `utf8`/`string` edge abuse | CWE-20 | library edge cases |
| CORE-024 | Bytecode verifier absent | CWE-94 | no verification of loaded chunks |
| CORE-025 | `mode="bt"` accepts bytecode | CWE-94 | `load` should restrict to text for untrusted input |

### Resource limits / DoS

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-026 | No instruction cap | CWE-400 | infinite loop hangs the VM |
| CORE-027 | No memory cap | CWE-400 | a script allocates until OOM |
| CORE-028 | Stack overflow via recursion | CWE-674 | deep Lua recursion |
| CORE-029 | C-stack overflow via pcall depth | CWE-674 | nested pcall/error |
| CORE-030 | String interning blowup | CWE-400 | many unique strings exhaust memory |
| CORE-031 | Table growth blowup | CWE-400 | a giant table |
| CORE-032 | GC pause / stop-the-world | CWE-400 | huge graphs cause long GC pauses |
| CORE-033 | Event-loop starvation | CWE-834 | long synchronous work blocks the loop |
| CORE-034 | Task-pool exhaustion | CWE-400 | many blocking tasks starve the pool |
| CORE-035 | Task-pool deadlock | CWE-833 | a task blocks on a result only the pool can produce |
| CORE-036 | Work-ledger leak | CWE-404 | retained pending-work keeps the loop alive |
| CORE-037 | Timer queue blowup | CWE-400 | unbounded timers |
| CORE-038 | Microtask starvation | CWE-834 | microtasks starve macrotasks |
| CORE-039 | Reentrant loop drive | CWE-674 | driving the loop from within a callback |
| CORE-040 | Unbounded posted-jobs queue | CWE-400 | `mainLoop().post` flood |

### Concurrency & lifecycle

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-041 | Teardown UAF (server refs) | CWE-416 | `lua_State` freed while a server holds refs |
| CORE-042 | Teardown order of modules | CWE-416 | a module freed before its dependents |
| CORE-043 | Cross-thread Lua access | CWE-362 | a worker touches the interpreter |
| CORE-044 | Global state race | CWE-362 | shared runtime state without a lock |
| CORE-045 | Double-stop runtime | CWE-675 | `stop()` called twice |
| CORE-046 | Stop during dispatch | CWE-362 | stop while a request runs |
| CORE-047 | Signal during GC | CWE-364 | signal handler runs mid-collection |
| CORE-048 | Worker outlives runtime | CWE-416 | a task references freed runtime state |
| CORE-049 | Promise outlives lua_State | CWE-416 | settle after the state is gone |
| CORE-050 | Atexit/finalizer ordering | CWE-416 | static destruction order issues |
| CORE-051 | Thread-pool shutdown join | CWE-833 | join hangs on a blocked worker |
| CORE-052 | Main-loop wake race | CWE-362 | wake vs idle-exit predicate |
| CORE-053 | Idle-exit with pending I/O | CWE-662 | loop exits with sockets/servers active |
| CORE-054 | Re-entrant module install | CWE-674 | installing modules during install |
| CORE-055 | Registry corruption | CWE-664 | concurrent registry ref ops |

### Host entry / config / env

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-056 | `arg` injection | CWE-88 | malicious argv interpreted as options |
| CORE-057 | Env var trust (`VARN_*`) | CWE-454 | `VARN_PORT`/TLS paths from a hostile env |
| CORE-058 | `VARN_PORT` overflow | CWE-190 | huge/negative port value |
| CORE-059 | TLS path from env | CWE-426 | attacker-controlled cert/key path |
| CORE-060 | Script path traversal | CWE-22 | the script path argument |
| CORE-061 | Working-directory assumption | CWE-22 | relative paths resolved unexpectedly |
| CORE-062 | Locale-dependent behavior | CWE-697 | locale changes number/string handling |
| CORE-063 | Unhandled top-level error | CWE-755 | uncaught error → clean non-zero exit |
| CORE-064 | Error message info leak | CWE-209 | top-level error reveals internals |
| CORE-065 | Exit-code correctness | CWE-754 | failure not reflected in the exit code |
| CORE-066 | Stdout/stderr injection | CWE-117 | control bytes in console output |
| CORE-067 | Huge script file | CWE-400 | loading a multi-GB script |
| CORE-068 | Script with NUL bytes | CWE-626 | NUL in the source |
| CORE-069 | BOM/shebang handling | CWE-20 | leading BOM/`#!` line |
| CORE-070 | Chunk-name injection | CWE-117 | crafted chunk name in errors/logs |

### Native-module registry & bindings

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-071 | Module install order | CWE-696 | dependency installed after a dependent |
| CORE-072 | Duplicate module registration | CWE-694 | a name registered twice |
| CORE-073 | Module name collision | CWE-694 | shadowing a built-in |
| CORE-074 | require returns cached | CWE-20 | stale cached module instance |
| CORE-075 | Metatable leak across modules | CWE-913 | a shared metatable mutated |
| CORE-076 | getRuntime pointer validity | CWE-825 | runtime pointer stale in a binding |
| CORE-077 | Light-userdata runtime ptr | CWE-843 | forged runtime pointer |
| CORE-078 | Binding arg count drift | CWE-20 | bindings disagree on arity |
| CORE-079 | Shared buffer across bindings | CWE-664 | a reused scratch buffer races |
| CORE-080 | Global function override | CWE-913 | a script replaces a module function |

### Memory safety & fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-081 | Engine OOM handling | CWE-400 | Lua allocator failure handled |
| CORE-082 | Allocator integer overflow | CWE-190 | growth math overflow |
| CORE-083 | C-API misuse crash | CWE-20 | a binding misuses the C API |
| CORE-084 | pcall boundary leak | CWE-401 | error unwinding leaks resources |
| CORE-085 | longjmp over destructors | CWE-755 | `luaL_error` skips RAII in C builds |
| CORE-086 | Exception across boundary | CWE-248 | C++ throw reaches the interpreter |
| CORE-087 | Stack overflow detection | CWE-674 | Lua stack limit enforced |
| CORE-088 | Recursive metamethod | CWE-674 | `__index`/`__add` recursion |
| CORE-089 | GC finalizer crash | CWE-416 | `__gc` errors handled |
| CORE-090 | Weak-table abuse | CWE-401 | weak references defeat cleanup |
| CORE-091 | Coroutine across states | CWE-362 | moving a coroutine between states |
| CORE-092 | xmove between states | CWE-664 | unsafe value transfer |
| CORE-093 | Registry index exhaustion | CWE-400 | endless `luaL_ref` |
| CORE-094 | Upvalue index OOB | CWE-125 | `lua_upvalueindex` misuse |
| CORE-095 | Userdata size mismatch | CWE-131 | wrong size in `lua_newuserdatauv` |
| CORE-096 | Uservalue slot OOB | CWE-125 | out-of-range uservalue index |
| CORE-097 | Light vs full userdata mix | CWE-843 | confusion between userdata kinds |
| CORE-098 | Reentrancy in allocator | CWE-674 | allocation triggers GC re-entry |
| CORE-099 | Time/clock source abuse | CWE-682 | wall-clock jumps affect the runtime |
| CORE-100 | Fuzz scripts + bindings | CWE-20 | random scripts/argv/env never crash the host |

---

## Additional cases (deeper / documented)

### Lua VM / sandbox (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-101 | Redis Lua sandbox escape | CVE-2022-0543 | `package`/`luaopen_*` reachable → RCE |
| CORE-102 | `string.rep` OOM | CWE-789 | `("x"):rep(2^40)` exhausts memory |
| CORE-103 | `table.concat`/insert blowup | CWE-400 | quadratic table building |
| CORE-104 | `string.format` `%` abuse | CWE-134 | format-spec DoS / type confusion |
| CORE-105 | `string.pack` size overflow | CWE-190 | pack with huge counts |
| CORE-106 | Pattern matching ReDoS | CWE-1333 | `string.find` with a pathological pattern |
| CORE-107 | `string.gsub` replacement blowup | CWE-400 | exponential replacement growth |
| CORE-108 | `utf8` library edge | CWE-176 | malformed sequences |
| CORE-109 | `tonumber` base abuse | CWE-20 | exotic bases / huge numbers |
| CORE-110 | `select('#', ...)` huge varargs | CWE-400 | argument flood |
| CORE-111 | Metatable `__index` chain | CWE-674 | deep/cyclic metatable chain |
| CORE-112 | `__gc` resurrection | CWE-416 | finalizer revives an object |
| CORE-113 | `__gc` error handling | CWE-755 | throwing finalizer |
| CORE-114 | Weak-table cleanup defeat | CWE-401 | strong ref defeats weakness |
| CORE-115 | Coroutine across states | CWE-362 | moving a coroutine between states |
| CORE-116 | `coroutine.close` misuse | CWE-664 | closing a running coroutine |
| CORE-117 | Stack-overflow recovery | CWE-674 | recovery leaves inconsistent state |
| CORE-118 | `pcall` swallows fatal | CWE-390 | catching a non-recoverable error |
| CORE-119 | `xpcall` handler abuse | CWE-674 | handler recurses on error |
| CORE-120 | `error` with a table object | CWE-755 | non-string error propagation |
| CORE-121 | `error` level confusion | CWE-209 | wrong source attribution |
| CORE-122 | Upvalue sharing leak | CWE-913 | closures share mutable upvalues |
| CORE-123 | `_ENV` manipulation | CWE-913 | swapping the environment |
| CORE-124 | Integer/float key aliasing | CWE-704 | `t[1]`/`t[1.0]` |
| CORE-125 | `rawset`/`rawget` bypass | CWE-913 | bypassing protective metamethods |

### Bytecode / code loading (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-126 | Crafted bytecode load | CWE-94 | malicious chunk subverts the VM (no verifier) |
| CORE-127 | `load` mode `bt` accepts binary | CWE-94 | should be text-only for untrusted input |
| CORE-128 | `string.dump` disclosure | CWE-200 | dumping function bytecode |
| CORE-129 | Truncated bytecode | CWE-20 | malformed precompiled chunk |
| CORE-130 | Bytecode version mismatch | CWE-20 | chunk from another Lua version |
| CORE-131 | Endianness in bytecode | CWE-188 | cross-arch bytecode |
| CORE-132 | Loadstring of attacker source | CWE-95 | eval of untrusted Lua |
| CORE-133 | `dofile`/`loadfile` path | CWE-73 | arbitrary file execution |
| CORE-134 | `require` cpath native load | CWE-829 | planted C module |
| CORE-135 | Chunk-name injection | CWE-117 | crafted chunk name in logs/errors |

### Resource limits (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-136 | No instruction-count cap | CWE-400 | infinite loop hangs the VM |
| CORE-137 | No memory cap (no allocator hook) | CWE-400 | script allocates to OOM |
| CORE-138 | Allocator hook bypass | CWE-693 | a cap that can be evaded |
| CORE-139 | GC disable via script | CWE-400 | `collectgarbage("stop")` |
| CORE-140 | GC step abuse | CWE-400 | forcing full collections |
| CORE-141 | String-intern table blowup | CWE-400 | many unique strings |
| CORE-142 | Deep recursion C-stack | CWE-674 | native stack overflow |
| CORE-143 | Pcall-depth C-stack | CWE-674 | nested pcall overflow |
| CORE-144 | Metamethod recursion | CWE-674 | `__index`/`__add` loops |
| CORE-145 | Coroutine count blowup | CWE-400 | unbounded coroutines |
| CORE-146 | Registry index exhaustion | CWE-400 | endless `luaL_ref` |
| CORE-147 | Userdata growth blowup | CWE-400 | many userdata objects |

### Event loop / task pool (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-148 | Event-loop starvation | CWE-834 | long sync work blocks the loop |
| CORE-149 | Task-pool exhaustion | CWE-400 | blocking tasks starve the pool |
| CORE-150 | Task-pool deadlock | CWE-833 | task awaits pool-produced result |
| CORE-151 | Posted-jobs queue flood | CWE-400 | unbounded `post` queue |
| CORE-152 | Work-ledger leak keeps alive | CWE-404 | loop never exits |
| CORE-153 | Idle-exit with active server | CWE-662 | premature exit |
| CORE-154 | Wake/idle predicate race | CWE-362 | wake vs idle-exit |
| CORE-155 | Reentrant loop run | CWE-674 | `run()` within a callback |
| CORE-156 | Timer + I/O fairness | CWE-696 | starvation between sources |

### Lifecycle / teardown (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-157 | Teardown UAF (server refs) | CWE-416 | `lua_State` freed with refs held |
| CORE-158 | Module teardown order | CWE-416 | dependency freed first |
| CORE-159 | Static-init order fiasco | CWE-456 | global used before init |
| CORE-160 | Static-destruction use | CWE-416 | global used after destruction |
| CORE-161 | Worker outlives runtime | CWE-416 | task references freed state |
| CORE-162 | Promise outlives state | CWE-416 | settle after teardown |
| CORE-163 | Double-stop runtime | CWE-675 | `stop()` twice |
| CORE-164 | Stop during dispatch | CWE-362 | stop mid-request |
| CORE-165 | Thread-pool join hang | CWE-833 | join on a blocked worker |
| CORE-166 | Signal during GC | CWE-364 | handler runs mid-collection |
| CORE-167 | atexit/finalizer ordering | CWE-416 | static dtor order |

### Host entry / env / config (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-168 | argv injection | CWE-88 | argv parsed as options |
| CORE-169 | Env trust (`VARN_*`) | CWE-454 | hostile env alters config |
| CORE-170 | `VARN_PORT` overflow/negative | CWE-190 | bad port value |
| CORE-171 | TLS path from env | CWE-426 | attacker cert/key path |
| CORE-172 | Script path traversal | CWE-22 | the script-path argument |
| CORE-173 | cwd assumption | CWE-22 | relative paths resolved unexpectedly |
| CORE-174 | Locale-dependent behavior | CWE-697 | number/case handling by locale |
| CORE-175 | Huge script file | CWE-400 | multi-GB source |
| CORE-176 | NUL in source | CWE-626 | source truncation |
| CORE-177 | BOM/shebang handling | CWE-20 | leading BOM/`#!` |
| CORE-178 | Stdout/stderr control bytes | CWE-117 | console injection |
| CORE-179 | Exit-code correctness | CWE-754 | failure not reflected |
| CORE-180 | Top-level error info leak | CWE-209 | error reveals internals |

### Native registry / bindings / memory (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-181 | Module install order | CWE-696 | dependency installed late |
| CORE-182 | Duplicate registration | CWE-694 | a name registered twice |
| CORE-183 | Built-in shadowing | CWE-694 | a script replaces a module fn |
| CORE-184 | getRuntime pointer stale | CWE-825 | runtime pointer invalid in a binding |
| CORE-185 | Forged light-userdata runtime ptr | CWE-843 | fake runtime pointer |
| CORE-186 | Shared scratch buffer race | CWE-664 | reused buffer across bindings |
| CORE-187 | Userdata size mismatch | CWE-131 | wrong `lua_newuserdatauv` size |
| CORE-188 | Uservalue slot OOB | CWE-125 | bad uservalue index |
| CORE-189 | Upvalue index OOB | CWE-125 | `lua_upvalueindex` misuse |
| CORE-190 | xmove between states | CWE-664 | unsafe value transfer |
| CORE-191 | Allocator integer overflow | CWE-190 | growth math overflow |
| CORE-192 | Engine OOM handling | CWE-400 | allocator failure handled |
| CORE-193 | longjmp over C++ dtors | CWE-755 | `luaL_error` skips RAII |
| CORE-194 | Exception across boundary | CWE-248 | C++ throw reaches the VM |
| CORE-195 | pcall boundary resource leak | CWE-401 | unwinding leaks resources |
| CORE-196 | Weak-table finalization order | CWE-416 | finalize order surprise |
| CORE-197 | GC finalizer crash | CWE-416 | `__gc` errors |
| CORE-198 | ASan/UBSan trip in core | CWE-125 | sanitizer finding |
| CORE-199 | Differential dummy/real driver | CWE-697 | stub vs real diverge on safety |
| CORE-200 | Fuzz scripts + argv + env | CWE-20 | random inputs never crash the host |

---

## Round 3 — deeper / documented

### Lua VM internals (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-201 | Bytecode load without verifier | CWE-94 | crafted chunk subverts the VM |
| CORE-202 | `string.dump` of a C function | CWE-200 | bytecode disclosure |
| CORE-203 | Truncated/garbage bytecode | CWE-20 | malformed precompiled chunk |
| CORE-204 | Cross-version bytecode | CWE-20 | chunk from another Lua version |
| CORE-205 | Endian/size bytecode mismatch | CWE-188 | cross-arch chunk |
| CORE-206 | `load` mode allows binary | CWE-94 | should be text-only for untrusted |
| CORE-207 | Pattern-matching ReDoS | CWE-1333 | `string.find` pathological pattern |
| CORE-208 | `string.rep` OOM | CWE-789 | huge repetition |
| CORE-209 | `string.format` `%` abuse | CWE-134 | format-spec DoS |
| CORE-210 | `string.pack` count overflow | CWE-190 | huge pack count |
| CORE-211 | `string.gsub` blowup | CWE-400 | exponential replacement |
| CORE-212 | `table.concat` quadratic | CWE-407 | quadratic build |
| CORE-213 | `tonumber` base abuse | CWE-20 | exotic base/huge number |
| CORE-214 | `utf8` library edge | CWE-176 | malformed sequences |
| CORE-215 | Metatable `__index` cycle | CWE-674 | cyclic metatable chain |
| CORE-216 | `__gc` resurrection | CWE-416 | finalizer revives an object |
| CORE-217 | `__gc` throwing | CWE-755 | finalizer error |
| CORE-218 | Weak-table cleanup defeat | CWE-401 | strong ref defeats weakness |
| CORE-219 | Coroutine across states | CWE-362 | moving a coroutine |
| CORE-220 | `coroutine.close` misuse | CWE-664 | closing a running coroutine |
| CORE-221 | `_ENV` swap | CWE-913 | environment manipulation |
| CORE-222 | `rawset`/`rawget` bypass | CWE-913 | bypass protective metamethods |
| CORE-223 | Base-type metatable poison | CWE-913 | string/number metatable |
| CORE-224 | Integer/float key aliasing | CWE-704 | `t[1]` vs `t[1.0]` |
| CORE-225 | Upvalue sharing leak | CWE-913 | closures share mutable state |

### Sandbox / stdlib exposure (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-226 | `os.execute` reachable | CWE-78 | shell execution |
| CORE-227 | `io.popen` reachable | CWE-78 | command execution |
| CORE-228 | `io.open` arbitrary file | CWE-73 | bypass fs module |
| CORE-229 | `os.getenv` secret read | CWE-200 | env disclosure |
| CORE-230 | `os.exit` from script | CWE-20 | script kills the process |
| CORE-231 | `os.remove`/`rename` | CWE-73 | filesystem mutation |
| CORE-232 | `package.loadlib` | CWE-829 | load native code |
| CORE-233 | `require` native module | CWE-829 | planted C module |
| CORE-234 | `dofile`/`loadfile` | CWE-73 | execute arbitrary files |
| CORE-235 | `load`/`loadstring` text | CWE-95 | eval untrusted Lua |
| CORE-236 | `debug` library exposed | CWE-668 | introspection/tampering |
| CORE-237 | `debug.setmetatable` | CWE-913 | override type metatables |
| CORE-238 | `debug.getupvalue/setupvalue` | CWE-913 | closure-state access |
| CORE-239 | `collectgarbage` abuse | CWE-400 | GC churn / disable |
| CORE-240 | `package.path`/`cpath` injection | CWE-426 | search-path manipulation |
| CORE-241 | FFI reachable from script | CWE-829 | full native power |
| CORE-242 | Sandbox escape via coroutine | CWE-265 | coroutine internals |
| CORE-243 | Sandbox escape via error obj | CWE-265 | leak refs through errors |

### Resource limits (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-244 | No instruction-count cap | CWE-400 | infinite loop hangs the VM |
| CORE-245 | No memory cap (allocator hook) | CWE-400 | allocate to OOM |
| CORE-246 | Allocator-hook bypass | CWE-693 | evadable cap |
| CORE-247 | GC stop via script | CWE-400 | `collectgarbage("stop")` |
| CORE-248 | GC step abuse | CWE-400 | forced full collections |
| CORE-249 | String-intern blowup | CWE-400 | many unique strings |
| CORE-250 | Deep Lua recursion | CWE-674 | native stack overflow |
| CORE-251 | Nested pcall depth | CWE-674 | C-stack overflow |
| CORE-252 | Metamethod recursion | CWE-674 | `__index`/`__add` loop |
| CORE-253 | Coroutine count blowup | CWE-400 | unbounded coroutines |
| CORE-254 | Registry index exhaustion | CWE-400 | endless `luaL_ref` |
| CORE-255 | Userdata growth blowup | CWE-400 | many userdata |

### Event loop / task pool (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-256 | Event-loop starvation | CWE-834 | long sync work blocks the loop |
| CORE-257 | Task-pool exhaustion | CWE-400 | blocking tasks starve the pool |
| CORE-258 | Task-pool deadlock | CWE-833 | task awaits a pool-produced result |
| CORE-259 | Posted-jobs flood | CWE-400 | unbounded `post` queue |
| CORE-260 | Work-ledger leak | CWE-404 | loop never exits |
| CORE-261 | Idle-exit with active server | CWE-662 | premature exit |
| CORE-262 | Wake/idle predicate race | CWE-362 | wake vs idle-exit |
| CORE-263 | Reentrant loop run | CWE-674 | run within a callback |
| CORE-264 | Timer + I/O fairness | CWE-696 | source starvation |

### Lifecycle / teardown (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-265 | Teardown UAF (server refs) | CWE-416 | `lua_State` freed with refs held |
| CORE-266 | Module teardown order | CWE-416 | dependency freed first |
| CORE-267 | Static-init order fiasco | CWE-456 | global used before init |
| CORE-268 | Static-destruction use | CWE-416 | global used after destruction |
| CORE-269 | Worker outlives runtime | CWE-416 | task references freed state |
| CORE-270 | Promise outlives state | CWE-416 | settle after teardown |
| CORE-271 | Double-stop runtime | CWE-675 | `stop()` twice |
| CORE-272 | Stop during dispatch | CWE-362 | stop mid-request |
| CORE-273 | Thread-pool join hang | CWE-833 | join on a blocked worker |
| CORE-274 | Signal during GC | CWE-364 | handler mid-collection |
| CORE-275 | atexit/finalizer ordering | CWE-416 | static dtor order |

### Host entry / env / registry / memory (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CORE-276 | argv injection | CWE-88 | argv parsed as options |
| CORE-277 | Env trust (`VARN_*`) | CWE-454 | hostile env alters config |
| CORE-278 | `VARN_PORT` overflow/negative | CWE-190 | bad port value |
| CORE-279 | TLS path from env | CWE-426 | attacker cert/key path |
| CORE-280 | Script path traversal | CWE-22 | script-path argument |
| CORE-281 | cwd assumption | CWE-22 | relative path resolution |
| CORE-282 | Locale-dependent behavior | CWE-697 | number/case handling |
| CORE-283 | Huge script file | CWE-400 | multi-GB source |
| CORE-284 | NUL in source | CWE-626 | source truncation |
| CORE-285 | BOM/shebang handling | CWE-20 | leading BOM/`#!` |
| CORE-286 | Stdout/stderr control bytes | CWE-117 | console injection |
| CORE-287 | Exit-code correctness | CWE-754 | failure not reflected |
| CORE-288 | Top-level error info leak | CWE-209 | error reveals internals |
| CORE-289 | Duplicate module registration | CWE-694 | a name registered twice |
| CORE-290 | Built-in shadowing | CWE-694 | a script replaces a module fn |
| CORE-291 | getRuntime pointer stale | CWE-825 | invalid runtime pointer |
| CORE-292 | Forged light-userdata runtime ptr | CWE-843 | fake runtime pointer |
| CORE-293 | Userdata size mismatch | CWE-131 | wrong allocation size |
| CORE-294 | Uservalue/upvalue index OOB | CWE-125 | bad index |
| CORE-295 | xmove between states | CWE-664 | unsafe value transfer |
| CORE-296 | Allocator integer overflow | CWE-190 | growth math overflow |
| CORE-297 | longjmp over C++ dtors | CWE-755 | `luaL_error` skips RAII |
| CORE-298 | Exception across boundary | CWE-248 | C++ throw reaches the VM |
| CORE-299 | ASan/UBSan trip in core | CWE-125 | sanitizer finding |
| CORE-300 | Fuzz scripts + argv + env | CWE-20 | random inputs never crash the host |
