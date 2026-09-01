# ⏳ async

`async.sleep(ms)`, `async.spawn(fn)`, `async.run(fn)`, `promise:await()`,
`promise:isDone()`, and the promise resolve/reject machinery.

### Promise lifecycle & references

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-001 | Coroutine ref leak | CWE-401 | suspended coroutine refs never released |
| ASY-002 | Callback ref leak | CWE-401 | resolve/reject callback refs leak |
| ASY-003 | Double-unref | CWE-672 | a settled promise's slot freed twice |
| ASY-004 | Pending promise leak | CWE-401 | never-settled promises accumulate |
| ASY-005 | Promise GC while pending | CWE-416 | a referenced promise collected mid-flight |
| ASY-006 | Result value leak | CWE-401 | held result values never released |
| ASY-007 | Chain ref leak | CWE-401 | `:await` chain leaves dangling refs |
| ASY-008 | Self-referential promise | CWE-674 | a promise awaiting itself |
| ASY-009 | Circular await chain | CWE-833 | mutual awaits deadlock |
| ASY-010 | Finalizer of live promise | CWE-416 | `__gc` runs on an in-flight promise |

### Settlement correctness

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-011 | Double resolve | CWE-362 | resolving twice changes the value |
| ASY-012 | Double reject | CWE-362 | rejecting twice |
| ASY-013 | Resolve-then-reject | CWE-362 | second settle must be a no-op |
| ASY-014 | Reject-then-resolve | CWE-362 | order independence |
| ASY-015 | Settle after await returned | CWE-362 | late settle of a consumed promise |
| ASY-016 | Settle from multiple threads | CWE-362 | concurrent settle race |
| ASY-017 | Resolve with a promise | CWE-704 | resolving with another promise (chaining) |
| ASY-018 | Resolve with nil/false | CWE-20 | falsey result vs error distinction |
| ASY-019 | Reject with non-string | CWE-20 | error object/table as a reason |
| ASY-020 | Value type fidelity | CWE-704 | resolved value type preserved |

### Scheduling, threads & races

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-021 | Cross-thread Lua settle | CWE-362 | worker touches `lua_State` to settle |
| ASY-022 | Settle marshalled to loop | CWE-662 | settle posted to the main loop, not direct |
| ASY-023 | Resume on wrong thread | CWE-362 | coroutine resumed off the main loop |
| ASY-024 | Double-resume coroutine | CWE-362 | resuming a running/finished coroutine |
| ASY-025 | Resume after error | CWE-755 | resuming a coroutine that errored |
| ASY-026 | Reentrant scheduling | CWE-674 | spawning inside a resolve callback |
| ASY-027 | Ordering of callbacks | CWE-696 | resolve callbacks fire out of order |
| ASY-028 | Microtask vs macrotask order | CWE-696 | sleep(0) vs immediate ordering |
| ASY-029 | Timer ordering | CWE-696 | equal-delay sleeps fire in registration order |
| ASY-030 | Race spawn vs run exit | CWE-362 | spawn during entry-point teardown |
| ASY-031 | Settle during shutdown | CWE-362 | settle while the loop is stopping |
| ASY-032 | Lost wakeup | CWE-662 | notify races the wait predicate |
| ASY-033 | Work-ledger accounting race | CWE-362 | pending-work count under/overflow |
| ASY-034 | Premature loop exit | CWE-662 | loop exits with work still pending |
| ASY-035 | Background driver leak | CWE-404 | retain/release imbalance keeps the loop alive |

### Resource / OOM / starvation

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-036 | Busy-spin starvation | CWE-834 | tight `spawn` loop starves I/O |
| ASY-037 | Unbounded spawn | CWE-400 | spawning millions of coroutines |
| ASY-038 | Deep await chain | CWE-674 | thousands of nested awaits overflow |
| ASY-039 | sleep(0) flood | CWE-834 | zero-delay reschedule loop |
| ASY-040 | Huge sleep value | CWE-190 | `sleep(2^63)` overflow / never-fires |
| ASY-041 | Negative sleep | CWE-20 | `sleep(-1)` handling |
| ASY-042 | Non-numeric sleep | CWE-20 | string/table delay |
| ASY-043 | Timer accumulation | CWE-400 | many pending timers exhaust memory |
| ASY-044 | Coroutine stack growth | CWE-674 | deep recursion inside a coroutine |
| ASY-045 | Promise queue growth | CWE-400 | unbounded resolved-but-unawaited queue |
| ASY-046 | Fan-out amplification | CWE-405 | one event spawns exponential work |
| ASY-047 | Memory per pending promise | CWE-400 | many pending promises exhaust memory |

### Error handling & cancellation

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-048 | Unhandled rejection lost | CWE-755 | a rejection with no awaiter vanishes |
| ASY-049 | Unhandled rejection crash | CWE-755 | unhandled rejection aborts the process |
| ASY-050 | Error in spawned fn | CWE-755 | error escaping `spawn` is logged, not silent |
| ASY-051 | Error in run() entry | CWE-755 | uncaught error → non-zero exit |
| ASY-052 | Error in resolve callback | CWE-755 | callback throw isolated |
| ASY-053 | await returns nil,err | CWE-755 | failure surfaced as `nil, err` |
| ASY-054 | Error object info leak | CWE-209 | internal detail in the error reason |
| ASY-055 | No cancellation primitive | CWE-404 | a started op cannot be cancelled (leak) |
| ASY-056 | Cancel during settle race | CWE-362 | cancel races a settle |
| ASY-057 | Timeout vs completion race | CWE-362 | both fire near-simultaneously |
| ASY-058 | Exception across boundary | CWE-248 | C++ throw in the async core |
| ASY-059 | Stack imbalance on error | CWE-664 | error path leaves a balanced Lua stack |
| ASY-060 | Partial-result on error | CWE-459 | half-done state exposed |

### Misuse, fuzz & edges

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-061 | await outside coroutine | CWE-20 | `:await` on the main thread errors cleanly |
| ASY-062 | await an already-settled promise | CWE-20 | immediate value path |
| ASY-063 | await nil/non-promise | CWE-20 | awaiting a non-promise |
| ASY-064 | isDone as synchronization | CWE-367 | treating `isDone` as a lock |
| ASY-065 | isDone TOCTOU | CWE-367 | state changes after `isDone` |
| ASY-066 | spawn with non-function | CWE-20 | spawning a non-callable |
| ASY-067 | run() called twice | CWE-664 | nested/duplicate entry points |
| ASY-068 | run() inside run() | CWE-674 | re-entrant entry point |
| ASY-069 | spawn returns/ignored | CWE-252 | spawned promise dropped |
| ASY-070 | await in a finalizer | CWE-662 | awaiting during `__gc` |
| ASY-071 | Promise shared across threads | CWE-362 | a promise object used on two threads |
| ASY-072 | Resolve with huge value | CWE-400 | a multi-GB resolved value |
| ASY-073 | Chained then-callbacks depth | CWE-674 | deep callback chains |
| ASY-074 | Reentrant await of same promise | CWE-674 | two awaits on one promise |
| ASY-075 | Coroutine yield across C call | CWE-662 | yielding where the C frame can't resume |
| ASY-076 | Timer drift/precision | CWE-682 | accumulated drift over many sleeps |
| ASY-077 | Monotonic vs wall clock | CWE-682 | clock source affects scheduling |
| ASY-078 | Spawn storm under load | CWE-400 | request-driven coroutine explosion |
| ASY-079 | Promise identity confusion | CWE-843 | a non-promise userdata passed as a promise |
| ASY-080 | Type-confused userdata | CWE-843 | foreign userdata as a promise |
| ASY-081 | GC pressure under churn | CWE-401 | rapid create/settle leaks under GC |
| ASY-082 | Settle ordering fairness | CWE-696 | starvation of older promises |
| ASY-083 | Nested spawn lifetime | CWE-416 | child outlives parent's refs |
| ASY-084 | Error in close/cleanup | CWE-755 | cleanup error masks the result |
| ASY-085 | Loop wake spurious | CWE-662 | spurious wakeups handled |
| ASY-086 | Wall-clock jump (NTP) | CWE-682 | time jump breaks timers |
| ASY-087 | Huge fan-in (await many) | CWE-400 | awaiting thousands of promises |
| ASY-088 | Promise leak on early return | CWE-401 | function returns before awaiting |
| ASY-089 | Double await result | CWE-664 | awaiting twice returns stale |
| ASY-090 | Concurrent isDone race | CWE-362 | reading isDone while settling |
| ASY-091 | Resolve from timer callback | CWE-362 | timer-driven settle race |
| ASY-092 | Memory order on settle flag | CWE-1264 | missing barrier on the done flag |
| ASY-093 | Reentrancy in await resume | CWE-674 | resume triggers another await |
| ASY-094 | Exception in await continuation | CWE-755 | continuation throw handled |
| ASY-095 | Stack overflow via recursion | CWE-674 | recursive spawn/await |
| ASY-096 | Shutdown deadlock | CWE-833 | stop blocks on a pending await |
| ASY-097 | Orphaned coroutine memory | CWE-401 | never-resumed coroutine retained |
| ASY-098 | Error reason mutation | CWE-664 | shared error table mutated |
| ASY-099 | Info leak in unhandled log | CWE-532 | unhandled rejection logs secrets |
| ASY-100 | Fuzz scheduling sequences | CWE-20 | randomized spawn/sleep/await orders never crash |

---

## Additional cases (deeper / documented)

### Promise machinery (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-101 | Settle-after-free | CWE-416 | settle a promise whose state was collected |
| ASY-102 | Callback registry double-unref | CWE-672 | callback slot freed twice |
| ASY-103 | Resolve value retained forever | CWE-401 | result keeps a large object alive |
| ASY-104 | Reject reason retained | CWE-401 | error object pins memory |
| ASY-105 | then-chain leak | CWE-401 | each `:await` adds an unreleased ref |
| ASY-106 | Already-resolved fast path bug | CWE-664 | immediate value mis-handled |
| ASY-107 | Resolve order vs registration | CWE-696 | callbacks fire out of registration order |
| ASY-108 | Settle during callback iteration | CWE-362 | mutate the callback list while iterating |
| ASY-109 | Promise resolved with itself | CWE-674 | self-resolution loop |
| ASY-110 | Thenable adoption confusion | CWE-704 | resolving with a foreign thenable |
| ASY-111 | Resolve with multiple values | CWE-20 | extra return values dropped/mishandled |
| ASY-112 | nil vs error ambiguity | CWE-20 | `nil` result vs failure |
| ASY-113 | Error vs value tag confusion | CWE-704 | settled-state flag mismatch |
| ASY-114 | Settled-flag memory order | CWE-1264 | missing barrier on the done flag |
| ASY-115 | Spurious resolve from timer | CWE-362 | a stale timer settles a reused promise |

### Scheduling & timers (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-116 | Timer-wheel overflow | CWE-190 | huge delay overflows the schedule |
| ASY-117 | Negative-delay scheduling | CWE-20 | `sleep(-1)` behavior |
| ASY-118 | Zero-delay starvation | CWE-834 | `sleep(0)` reschedule loop |
| ASY-119 | Timer drift accumulation | CWE-682 | drift over many sleeps |
| ASY-120 | Wall-clock jump (NTP/DST) | CWE-682 | clock step breaks timers |
| ASY-121 | Monotonic vs wall source | CWE-682 | scheduling uses the wrong clock |
| ASY-122 | Equal-deadline ordering | CWE-696 | FIFO fairness at equal delays |
| ASY-123 | Timer cancellation race | CWE-362 | cancel vs fire |
| ASY-124 | Massive timer queue | CWE-400 | millions of pending timers |
| ASY-125 | Reschedule storm | CWE-834 | callback reschedules itself tightly |
| ASY-126 | Priority inversion in scheduler | CWE-833 | low-priority work blocks high |
| ASY-127 | Microtask vs macrotask order | CWE-696 | ordering guarantees |
| ASY-128 | Loop idle-exit with pending timer | CWE-662 | premature exit |

### Threads / races (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-129 | Worker settles off main loop | CWE-362 | direct Lua touch from a worker |
| ASY-130 | Resume on a foreign thread | CWE-362 | coroutine resumed off-thread |
| ASY-131 | Double-resume race | CWE-362 | two resumes of one coroutine |
| ASY-132 | xmove during settle | CWE-664 | value moved between states mid-settle |
| ASY-133 | Work-ledger underflow | CWE-191 | pending count goes negative |
| ASY-134 | Work-ledger overflow | CWE-190 | pending count wraps |
| ASY-135 | Background-driver retain leak | CWE-404 | retain without release |
| ASY-136 | Lost wakeup | CWE-662 | notify before wait |
| ASY-137 | Spurious wakeup mishandled | CWE-662 | wake without the predicate |
| ASY-138 | Shutdown vs settle race | CWE-362 | settle while stopping |
| ASY-139 | Shutdown deadlock on await | CWE-833 | stop blocks on a pending await |
| ASY-140 | Reentrant loop drive | CWE-674 | driving the loop within a callback |
| ASY-141 | TSan race on promise state | CWE-362 | data race flagged |

### Resource / OOM (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-142 | Coroutine explosion | CWE-400 | unbounded `spawn` |
| ASY-143 | Coroutine stack growth | CWE-674 | deep recursion in a coroutine |
| ASY-144 | Deep await chain overflow | CWE-674 | thousands of nested awaits |
| ASY-145 | Pending-promise accumulation | CWE-400 | never-settled promises pile up |
| ASY-146 | Resolved-unawaited queue growth | CWE-400 | results buffered unbounded |
| ASY-147 | Fan-out amplification | CWE-405 | one event spawns exponential work |
| ASY-148 | Huge resolved value | CWE-400 | multi-GB result |
| ASY-149 | GC pressure under churn | CWE-401 | rapid create/settle leaks |
| ASY-150 | Orphaned coroutine retained | CWE-401 | never-resumed coroutine pinned |
| ASY-151 | Busy-spin starves I/O | CWE-834 | tight loop blocks sockets/timers |
| ASY-152 | Memory per pending op | CWE-400 | per-op buffers exhaust memory |

### Error handling & cancellation (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-153 | Unhandled rejection lost | CWE-755 | silent swallow (Node-class) |
| ASY-154 | Unhandled rejection crash | CWE-755 | process abort |
| ASY-155 | Error in spawned fn lost | CWE-755 | not logged/surfaced |
| ASY-156 | run() error → exit code | CWE-754 | uncaught → nonzero exit |
| ASY-157 | Error in resolve callback | CWE-755 | callback throw isolated |
| ASY-158 | Error in finalizer | CWE-248 | throwing `__gc` |
| ASY-159 | Error reason mutated (shared) | CWE-664 | shared error table mutated |
| ASY-160 | Error info leak | CWE-209 | internal detail in the reason |
| ASY-161 | No cancellation → leak | CWE-404 | started op cannot be cancelled |
| ASY-162 | Cancel during settle race | CWE-362 | cancel vs settle |
| ASY-163 | Timeout vs completion race | CWE-362 | both fire near-simultaneously |
| ASY-164 | Partial result on error | CWE-459 | half-done state exposed |
| ASY-165 | Exception across boundary | CWE-248 | C++ throw in the async core |
| ASY-166 | Stack imbalance on error | CWE-664 | error path balanced |
| ASY-167 | Double-settle masks error | CWE-390 | second settle hides the first |

### Misuse / fuzz / edges (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-168 | await on main thread | CWE-20 | clean error, no crash |
| ASY-169 | await a non-promise | CWE-20 | non-promise argument |
| ASY-170 | await nil | CWE-20 | nil argument |
| ASY-171 | isDone as a lock | CWE-367 | treating it as synchronization |
| ASY-172 | isDone TOCTOU | CWE-367 | state changes after the check |
| ASY-173 | spawn non-function | CWE-20 | non-callable |
| ASY-174 | run() twice | CWE-664 | duplicate entry point |
| ASY-175 | run() within run() | CWE-674 | nested entry |
| ASY-176 | spawn result ignored | CWE-252 | dropped promise |
| ASY-177 | await in `__gc` | CWE-662 | awaiting during finalization |
| ASY-178 | promise shared across threads | CWE-362 | one object, two threads |
| ASY-179 | chained-then depth | CWE-674 | deep callback chain |
| ASY-180 | double await of one promise | CWE-664 | two awaits, stale result |
| ASY-181 | yield across a C call | CWE-662 | unresumable C frame |
| ASY-182 | huge fan-in await | CWE-400 | awaiting thousands |
| ASY-183 | promise leak on early return | CWE-401 | returns before awaiting |
| ASY-184 | type-confused userdata as promise | CWE-843 | foreign userdata |
| ASY-185 | reentrant await resume | CWE-674 | resume triggers another await |
| ASY-186 | exception in continuation | CWE-755 | continuation throw |
| ASY-187 | settle from a signal handler | CWE-364 | async-unsafe settle |
| ASY-188 | resolve in a tight timer loop | CWE-834 | timer-driven starvation |
| ASY-189 | nondeterministic settle order | CWE-696 | fairness/starvation |
| ASY-190 | promise identity reuse | CWE-664 | a recycled promise object |
| ASY-191 | error log leaks a secret | CWE-532 | unhandled rejection logs a token |
| ASY-192 | settle value NUL safety | CWE-626 | binary result fidelity |
| ASY-193 | huge sleep value | CWE-190 | overflow / never-fires |
| ASY-194 | non-numeric sleep | CWE-20 | string/table delay |
| ASY-195 | spawn during shutdown | CWE-362 | spawn while stopping |
| ASY-196 | nested-spawn lifetime | CWE-416 | child outlives parent refs |
| ASY-197 | promise queue fairness | CWE-696 | older promises starved |
| ASY-198 | differential vs reference loop | CWE-697 | behavior diverges from a known runtime |
| ASY-199 | ASan/TSan under churn | CWE-416 | sanitizer trip under load |
| ASY-200 | Fuzz scheduling sequences | CWE-20 | random spawn/sleep/await orders never crash |
