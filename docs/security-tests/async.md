# ⏳ async

The calls `async.sleep(ms)`, `async.spawn(fn)`, `async.run(fn)`, `promise:await()`, `promise:isDone()`, and the promise resolve/reject machinery are covered here.

### Promise lifecycle & references

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-001 | Coroutine ref leak | CWE-401 | Suspended coroutine refs never released |
| ASY-002 | Callback ref leak | CWE-401 | Resolve/reject callback refs leak |
| ASY-003 | Double-unref | CWE-672 | A settled promise's slot freed twice |
| ASY-004 | Pending promise leak | CWE-401 | Never-settled promises accumulate |
| ASY-005 | Promise GC while pending | CWE-416 | A referenced promise collected mid-flight |
| ASY-006 | Result value leak | CWE-401 | Held result values never released |
| ASY-007 | Chain ref leak | CWE-401 | An `:await` chain leaves dangling refs |
| ASY-008 | Self-referential promise | CWE-674 | A promise awaiting itself |
| ASY-009 | Circular await chain | CWE-833 | Mutual awaits deadlock |
| ASY-010 | Finalizer of live promise | CWE-416 | The `__gc` finalizer runs on an in-flight promise |

### Settlement correctness

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-011 | Double resolve | CWE-362 | Resolving twice changes the value |
| ASY-012 | Double reject | CWE-362 | Rejecting twice |
| ASY-013 | Resolve-then-reject | CWE-362 | Second settle must be a no-op |
| ASY-014 | Reject-then-resolve | CWE-362 | Order independence |
| ASY-015 | Settle after await returned | CWE-362 | Late settle of a consumed promise |
| ASY-016 | Settle from multiple threads | CWE-362 | Concurrent settle race |
| ASY-017 | Resolve with a promise | CWE-704 | Resolving with another promise (chaining) |
| ASY-018 | Resolve with `nil`/`false` | CWE-20 | Falsey result vs error distinction |
| ASY-019 | Reject with non-string | CWE-20 | Error object/table as a reason |
| ASY-020 | Value type fidelity | CWE-704 | Resolved value type preserved |

### Scheduling, threads & races

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-021 | Cross-thread Lua settle | CWE-362 | Worker touches `lua_State` to settle |
| ASY-022 | Settle marshalled to loop | CWE-662 | Settle posted to the main loop, not direct |
| ASY-023 | Resume on wrong thread | CWE-362 | Coroutine resumed off the main loop |
| ASY-024 | Double-resume coroutine | CWE-362 | Resuming a running/finished coroutine |
| ASY-025 | Resume after error | CWE-755 | Resuming a coroutine that errored |
| ASY-026 | Reentrant scheduling | CWE-674 | Spawning inside a resolve callback |
| ASY-027 | Ordering of callbacks | CWE-696 | Resolve callbacks fire out of order |
| ASY-028 | Microtask vs macrotask order | CWE-696 | Ordering of `sleep(0)` vs immediate |
| ASY-029 | Timer ordering | CWE-696 | Equal-delay sleeps fire in registration order |
| ASY-030 | Race spawn vs run exit | CWE-362 | Spawn during entry-point teardown |
| ASY-031 | Settle during shutdown | CWE-362 | Settle while the loop is stopping |
| ASY-032 | Lost wakeup | CWE-662 | Notify races the wait predicate |
| ASY-033 | Work-ledger accounting race | CWE-362 | Pending-work count under/overflow |
| ASY-034 | Premature loop exit | CWE-662 | Loop exits with work still pending |
| ASY-035 | Background driver leak | CWE-404 | Retain/release imbalance keeps the loop alive |

### Resource / OOM / starvation

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-036 | Busy-spin starvation | CWE-834 | Tight `spawn` loop starves I/O |
| ASY-037 | Unbounded spawn | CWE-400 | Spawning millions of coroutines |
| ASY-038 | Deep await chain | CWE-674 | Thousands of nested awaits overflow |
| ASY-039 | `sleep(0)` flood | CWE-834 | Zero-delay reschedule loop |
| ASY-040 | Huge sleep value | CWE-190 | Overflow / never-fires on `sleep(2^63)` |
| ASY-041 | Negative sleep | CWE-20 | Handling of `sleep(-1)` |
| ASY-042 | Non-numeric sleep | CWE-20 | String/table delay |
| ASY-043 | Timer accumulation | CWE-400 | Many pending timers exhaust memory |
| ASY-044 | Coroutine stack growth | CWE-674 | Deep recursion inside a coroutine |
| ASY-045 | Promise queue growth | CWE-400 | Unbounded resolved-but-unawaited queue |
| ASY-046 | Fan-out amplification | CWE-405 | One event spawns exponential work |
| ASY-047 | Memory per pending promise | CWE-400 | Many pending promises exhaust memory |

### Error handling & cancellation

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-048 | Unhandled rejection lost | CWE-755 | A rejection with no awaiter vanishes |
| ASY-049 | Unhandled rejection crash | CWE-755 | Unhandled rejection aborts the process |
| ASY-050 | Error in spawned fn | CWE-755 | Error escaping `spawn` is logged, not silent |
| ASY-051 | Error in `run()` entry | CWE-755 | Uncaught error → non-zero exit |
| ASY-052 | Error in resolve callback | CWE-755 | Callback throw isolated |
| ASY-053 | `await` returns `nil,err` | CWE-755 | Failure surfaced as `nil, err` |
| ASY-054 | Error object info leak | CWE-209 | Internal detail in the error reason |
| ASY-055 | No cancellation primitive | CWE-404 | A started op cannot be cancelled (leak) |
| ASY-056 | Cancel during settle race | CWE-362 | Cancel races a settle |
| ASY-057 | Timeout vs completion race | CWE-362 | Both fire near-simultaneously |
| ASY-058 | Exception across boundary | CWE-248 | C++ throw in the async core |
| ASY-059 | Stack imbalance on error | CWE-664 | Error path leaves a balanced Lua stack |
| ASY-060 | Partial-result on error | CWE-459 | Half-done state exposed |

### Misuse, fuzz & edges

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-061 | `await` outside coroutine | CWE-20 | Calling `:await` on the main thread errors cleanly |
| ASY-062 | `await` an already-settled promise | CWE-20 | Immediate value path |
| ASY-063 | `await` `nil`/non-promise | CWE-20 | Awaiting a non-promise |
| ASY-064 | `isDone` as synchronization | CWE-367 | Treating `isDone` as a lock |
| ASY-065 | `isDone` TOCTOU | CWE-367 | State changes after `isDone` |
| ASY-066 | `spawn` with non-function | CWE-20 | Spawning a non-callable |
| ASY-067 | `run()` called twice | CWE-664 | Nested/duplicate entry points |
| ASY-068 | `run()` inside `run()` | CWE-674 | Re-entrant entry point |
| ASY-069 | `spawn` returns/ignored | CWE-252 | Spawned promise dropped |
| ASY-070 | await in a finalizer | CWE-662 | Awaiting during `__gc` |
| ASY-071 | Promise shared across threads | CWE-362 | A promise object used on two threads |
| ASY-072 | Resolve with huge value | CWE-400 | A multi-GB resolved value |
| ASY-073 | Chained then-callbacks depth | CWE-674 | Deep callback chains |
| ASY-074 | Reentrant await of same promise | CWE-674 | Two awaits on one promise |
| ASY-075 | Coroutine yield across C call | CWE-662 | Yielding where the C frame can't resume |
| ASY-076 | Timer drift/precision | CWE-682 | Accumulated drift over many sleeps |
| ASY-077 | Monotonic vs wall clock | CWE-682 | Clock source affects scheduling |
| ASY-078 | Spawn storm under load | CWE-400 | Request-driven coroutine explosion |
| ASY-079 | Promise identity confusion | CWE-843 | A non-promise userdata passed as a promise |
| ASY-080 | Type-confused userdata | CWE-843 | Foreign userdata as a promise |
| ASY-081 | GC pressure under churn | CWE-401 | Rapid create/settle leaks under GC |
| ASY-082 | Settle ordering fairness | CWE-696 | Starvation of older promises |
| ASY-083 | Nested spawn lifetime | CWE-416 | Child outlives parent's refs |
| ASY-084 | Error in close/cleanup | CWE-755 | Cleanup error masks the result |
| ASY-085 | Loop wake spurious | CWE-662 | Spurious wakeups handled |
| ASY-086 | Wall-clock jump (NTP) | CWE-682 | Time jump breaks timers |
| ASY-087 | Huge fan-in (await many) | CWE-400 | Awaiting thousands of promises |
| ASY-088 | Promise leak on early return | CWE-401 | Function returns before awaiting |
| ASY-089 | Double await result | CWE-664 | Awaiting twice returns stale |
| ASY-090 | Concurrent `isDone` race | CWE-362 | Reading `isDone` while settling |
| ASY-091 | Resolve from timer callback | CWE-362 | Timer-driven settle race |
| ASY-092 | Memory order on settle flag | CWE-1264 | Missing barrier on the done flag |
| ASY-093 | Reentrancy in await resume | CWE-674 | Resume triggers another await |
| ASY-094 | Exception in await continuation | CWE-755 | Continuation throw handled |
| ASY-095 | Stack overflow via recursion | CWE-674 | Recursive spawn/await |
| ASY-096 | Shutdown deadlock | CWE-833 | A stop blocks on a pending await |
| ASY-097 | Orphaned coroutine memory | CWE-401 | Never-resumed coroutine retained |
| ASY-098 | Error reason mutation | CWE-664 | Shared error table mutated |
| ASY-099 | Info leak in unhandled log | CWE-532 | Unhandled rejection logs secrets |
| ASY-100 | Fuzz scheduling sequences | CWE-20 | Randomized spawn/sleep/await orders never crash |

---

## Additional cases (deeper / documented)

### Promise machinery (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-101 | Settle-after-free | CWE-416 | Settle a promise whose state was collected |
| ASY-102 | Callback registry double-unref | CWE-672 | Callback slot freed twice |
| ASY-103 | Resolve value retained forever | CWE-401 | Result keeps a large object alive |
| ASY-104 | Reject reason retained | CWE-401 | Error object pins memory |
| ASY-105 | then-chain leak | CWE-401 | Each `:await` adds an unreleased ref |
| ASY-106 | Already-resolved fast path bug | CWE-664 | Immediate value mis-handled |
| ASY-107 | Resolve order vs registration | CWE-696 | Callbacks fire out of registration order |
| ASY-108 | Settle during callback iteration | CWE-362 | Mutate the callback list while iterating |
| ASY-109 | Promise resolved with itself | CWE-674 | Self-resolution loop |
| ASY-110 | Thenable adoption confusion | CWE-704 | Resolving with a foreign thenable |
| ASY-111 | Resolve with multiple values | CWE-20 | Extra return values dropped/mishandled |
| ASY-112 | `nil` vs error ambiguity | CWE-20 | A `nil` result vs failure |
| ASY-113 | Error vs value tag confusion | CWE-704 | Settled-state flag mismatch |
| ASY-114 | Settled-flag memory order | CWE-1264 | Missing barrier on the done flag |
| ASY-115 | Spurious resolve from timer | CWE-362 | A stale timer settles a reused promise |

### Scheduling & timers (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-116 | Timer-wheel overflow | CWE-190 | Huge delay overflows the schedule |
| ASY-117 | Negative-delay scheduling | CWE-20 | Behavior of `sleep(-1)` |
| ASY-118 | Zero-delay starvation | CWE-834 | A `sleep(0)` reschedule loop |
| ASY-119 | Timer drift accumulation | CWE-682 | Drift over many sleeps |
| ASY-120 | Wall-clock jump (NTP/DST) | CWE-682 | Clock step breaks timers |
| ASY-121 | Monotonic vs wall source | CWE-682 | Scheduling uses the wrong clock |
| ASY-122 | Equal-deadline ordering | CWE-696 | FIFO fairness at equal delays |
| ASY-123 | Timer cancellation race | CWE-362 | Cancel vs fire |
| ASY-124 | Massive timer queue | CWE-400 | Millions of pending timers |
| ASY-125 | Reschedule storm | CWE-834 | Callback reschedules itself tightly |
| ASY-126 | Priority inversion in scheduler | CWE-833 | Low-priority work blocks high |
| ASY-127 | Microtask vs macrotask order | CWE-696 | Ordering guarantees |
| ASY-128 | Loop idle-exit with pending timer | CWE-662 | Premature exit |

### Threads / races (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-129 | Worker settles off main loop | CWE-362 | Direct Lua touch from a worker |
| ASY-130 | Resume on a foreign thread | CWE-362 | Coroutine resumed off-thread |
| ASY-131 | Double-resume race | CWE-362 | Two resumes of one coroutine |
| ASY-132 | `xmove` during settle | CWE-664 | Value moved between states mid-settle |
| ASY-133 | Work-ledger underflow | CWE-191 | Pending count goes negative |
| ASY-134 | Work-ledger overflow | CWE-190 | Pending count wraps |
| ASY-135 | Background-driver retain leak | CWE-404 | Retain without release |
| ASY-136 | Lost wakeup | CWE-662 | Notify before wait |
| ASY-137 | Spurious wakeup mishandled | CWE-662 | Wake without the predicate |
| ASY-138 | Shutdown vs settle race | CWE-362 | Settle while stopping |
| ASY-139 | Shutdown deadlock on await | CWE-833 | A stop blocks on a pending await |
| ASY-140 | Reentrant loop drive | CWE-674 | Driving the loop within a callback |
| ASY-141 | TSan race on promise state | CWE-362 | Data race flagged |

### Resource / OOM (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-142 | Coroutine explosion | CWE-400 | Unbounded `spawn` |
| ASY-143 | Coroutine stack growth | CWE-674 | Deep recursion in a coroutine |
| ASY-144 | Deep await chain overflow | CWE-674 | Thousands of nested awaits |
| ASY-145 | Pending-promise accumulation | CWE-400 | Never-settled promises pile up |
| ASY-146 | Resolved-unawaited queue growth | CWE-400 | Results buffered unbounded |
| ASY-147 | Fan-out amplification | CWE-405 | One event spawns exponential work |
| ASY-148 | Huge resolved value | CWE-400 | Multi-GB result |
| ASY-149 | GC pressure under churn | CWE-401 | Rapid create/settle leaks |
| ASY-150 | Orphaned coroutine retained | CWE-401 | Never-resumed coroutine pinned |
| ASY-151 | Busy-spin starves I/O | CWE-834 | Tight loop blocks sockets/timers |
| ASY-152 | Memory per pending op | CWE-400 | Per-op buffers exhaust memory |

### Error handling & cancellation (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-153 | Unhandled rejection lost | CWE-755 | Silent swallow (Node-class) |
| ASY-154 | Unhandled rejection crash | CWE-755 | Process abort |
| ASY-155 | Error in spawned fn lost | CWE-755 | Not logged/surfaced |
| ASY-156 | `run()` error → exit code | CWE-754 | Uncaught → nonzero exit |
| ASY-157 | Error in resolve callback | CWE-755 | Callback throw isolated |
| ASY-158 | Error in finalizer | CWE-248 | A throwing `__gc` |
| ASY-159 | Error reason mutated (shared) | CWE-664 | Shared error table mutated |
| ASY-160 | Error info leak | CWE-209 | Internal detail in the reason |
| ASY-161 | No cancellation → leak | CWE-404 | Started op cannot be cancelled |
| ASY-162 | Cancel during settle race | CWE-362 | Cancel vs settle |
| ASY-163 | Timeout vs completion race | CWE-362 | Both fire near-simultaneously |
| ASY-164 | Partial result on error | CWE-459 | Half-done state exposed |
| ASY-165 | Exception across boundary | CWE-248 | C++ throw in the async core |
| ASY-166 | Stack imbalance on error | CWE-664 | Error path balanced |
| ASY-167 | Double-settle masks error | CWE-390 | Second settle hides the first |

### Misuse / fuzz / edges (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| ASY-168 | `await` on main thread | CWE-20 | Clean error, no crash |
| ASY-169 | `await` a non-promise | CWE-20 | Non-promise argument |
| ASY-170 | `await` `nil` | CWE-20 | A `nil` argument |
| ASY-171 | `isDone` as a lock | CWE-367 | Treating it as synchronization |
| ASY-172 | `isDone` TOCTOU | CWE-367 | State changes after the check |
| ASY-173 | `spawn` non-function | CWE-20 | Non-callable |
| ASY-174 | `run()` twice | CWE-664 | Duplicate entry point |
| ASY-175 | `run()` within `run()` | CWE-674 | Nested entry |
| ASY-176 | `spawn` result ignored | CWE-252 | Dropped promise |
| ASY-177 | `await` in `__gc` | CWE-662 | Awaiting during finalization |
| ASY-178 | promise shared across threads | CWE-362 | One object, two threads |
| ASY-179 | chained-then depth | CWE-674 | Deep callback chain |
| ASY-180 | double await of one promise | CWE-664 | Two awaits, stale result |
| ASY-181 | yield across a C call | CWE-662 | Unresumable C frame |
| ASY-182 | huge fan-in await | CWE-400 | Awaiting thousands |
| ASY-183 | promise leak on early return | CWE-401 | Returns before awaiting |
| ASY-184 | type-confused userdata as promise | CWE-843 | Foreign userdata |
| ASY-185 | reentrant await resume | CWE-674 | Resume triggers another await |
| ASY-186 | exception in continuation | CWE-755 | Continuation throw |
| ASY-187 | settle from a signal handler | CWE-364 | Async-unsafe settle |
| ASY-188 | resolve in a tight timer loop | CWE-834 | Timer-driven starvation |
| ASY-189 | nondeterministic settle order | CWE-696 | Fairness/starvation |
| ASY-190 | promise identity reuse | CWE-664 | A recycled promise object |
| ASY-191 | error log leaks a secret | CWE-532 | Unhandled rejection logs a token |
| ASY-192 | settle value NUL safety | CWE-626 | Binary result fidelity |
| ASY-193 | huge sleep value | CWE-190 | Overflow / never-fires |
| ASY-194 | non-numeric sleep | CWE-20 | String/table delay |
| ASY-195 | spawn during shutdown | CWE-362 | Spawn while stopping |
| ASY-196 | nested-spawn lifetime | CWE-416 | Child outlives parent refs |
| ASY-197 | promise queue fairness | CWE-696 | Older promises starved |
| ASY-198 | differential vs reference loop | CWE-697 | Behavior diverges from a known runtime |
| ASY-199 | ASan/TSan under churn | CWE-416 | Sanitizer trip under load |
| ASY-200 | Fuzz scheduling sequences | CWE-20 | Random spawn/sleep/await orders never crash |
