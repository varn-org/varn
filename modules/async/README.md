# ⏳ async

Coroutine-based concurrency over the runtime's event loop. Async functions return promises you can `:await()`, plus combinators to run and coordinate many of them at once.

```lua
local async = require("async")

async.run(function()
    async.sleep(50):await()
    print("done")
end)
```

## Capabilities

| Function | What it does |
|---|---|
| `async.run(fn)` | Run `fn` as the program's async entry point and answer its task handle. The process exits when it returns, or non-zero if an error escapes. |
| `async.spawn(fn)` | Run `fn` as a background coroutine that can `:await()` promises, and answer its task handle. |
| `task.cancel()` | Stop a task for good at its next await, so nothing it awaits ever resumes it, and close its to-be-closed variables. |
| `async.onFailure(handler)` | Set the one handler that receives the error, the traceback, the frames and the kind of every failure no caller receives, a failed task, server handler or callback and a promise that rejects with no one observing it, which the loop then survives. |
| `async.promise(fn)` | Run `fn` as a coroutine and return a promise that resolves with its return value or rejects with its error. |
| `async.deferred()` | Return a pending promise plus a one-shot `resolve` function that settles it from elsewhere. If the `resolve` function is garbage-collected without being called, the pending promise is broken so its awaiters resume with an error instead of leaking. |
| `async.sleep(ms)` | A promise that resolves after `ms` milliseconds. |
| `async.yield()` | A promise that resolves in the next pass of the loop, so a long loop of work lets other tasks, timers and sockets run and a host that polls with a budget ends its frame. |
| `async.all(list)` | Resolve to an array of every promise's result in input order. Reject as soon as any input rejects. |
| `async.allSettled(list)` | Resolve to an array of `{ ok = true, value = ... }` / `{ ok = false, error = ... }` in input order. Never reject. |
| `async.race(list)` | Settle with the first input to settle, whether it resolves or rejects. |
| `async.any(list)` | Resolve with the first input to resolve. Reject only when every input rejects. |
| `async.timeout(promise, ms)` | Resolve with `promise`'s value, or reject with a timeout error if it does not settle within `ms` milliseconds, removing its timer once the promise settles first. |
| `async.mapLimit(list, limit, fn)` | Call `fn(item)` (returning a promise) with at most `limit` in flight, and resolve to the results array in input order. |
| `promise:await()` | Pause the current coroutine until the promise settles, then return the value (or `nil, err` on failure). |
| `promise:catch(fn)` | Observe the promise and answer a promise of what `fn(err)` returns when it rejects, or of its value when it resolves. |
| `promise:isDone()` | A boolean hint about whether the promise has settled. Not a synchronization primitive, and it does not observe the promise. |

## Reference and tests

- Full reference: [docs/lua-api/async.md](../../docs/lua-api/async.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
