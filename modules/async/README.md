# ⏳ async

Coroutine-based concurrency over the runtime's event loop. Async functions return promises you can
`:await()`, plus combinators to run and coordinate many of them at once.

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
| `async.run(fn)` | Run `fn` as the program's async entry point; the process exits when it returns, or non-zero if an error escapes. |
| `async.spawn(fn)` | Run `fn` as a background coroutine that can `:await()` promises. |
| `async.promise(fn)` | Run `fn` as a coroutine and return a promise that resolves with its return value or rejects with its error. |
| `async.deferred()` | Return a pending promise plus a one-shot `resolve` function that settles it from elsewhere; if the `resolve` function is garbage-collected without being called, the pending promise is broken so its awaiters resume with an error instead of leaking. |
| `async.sleep(ms)` | A promise that resolves after `ms` milliseconds. |
| `async.all(list)` | Resolve to an array of every promise's result in input order; reject as soon as any input rejects. |
| `async.allSettled(list)` | Resolve to an array of `{ ok = true, value = ... }` / `{ ok = false, error = ... }` in input order; never reject. |
| `async.race(list)` | Settle with the first input to settle, whether it resolves or rejects. |
| `async.any(list)` | Resolve with the first input to resolve; reject only when every input rejects. |
| `async.timeout(promise, ms)` | Resolve with `promise`'s value, or reject with a timeout error if it does not settle within `ms` milliseconds. |
| `async.mapLimit(list, limit, fn)` | Call `fn(item)` (returning a promise) with at most `limit` in flight, and resolve to the results array in input order. |
| `promise:await()` | Pause the current coroutine until the promise settles, then return the value (or `nil, err` on failure). |
| `promise:isDone()` | A boolean hint about whether the promise has settled; not a synchronization primitive. |

## Reference, examples, and tests

- Full reference: [docs/lua-api/async.md](../../docs/lua-api/async.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
