# ⏳ async

Coroutine-based concurrency over the event loop. Async functions return promises.

- `async.sleep(ms)` → a promise that resolves after `ms` milliseconds.
- `async.spawn(fn)` → run `fn` as a background coroutine that can `:await()` promises.
- `async.run(fn)` → run `fn` as the program's async entry point. The process exits when it returns, or with a non-zero status if an uncaught error escapes.
- `async.promise(fn)` → run `fn` as a coroutine and return a promise that resolves with its return value, or rejects with its error.
- `async.deferred()` → returns a promise plus a one-shot `resolve` function; calling `resolve()` settles the promise from elsewhere on the event loop. If the `resolve` function is garbage-collected without ever being called, the still-pending promise is broken (rejected) so its awaiters resume with an error rather than hanging forever. The building block for connection pools, semaphores, and other event-driven waits.

## Combinators

Each combinator returns a promise you can `:await()`.

- `async.all(list)` → resolves to an array of every promise's result in input order. Rejects as soon as any input rejects.
- `async.allSettled(list)` → resolves to an array of `{ ok = true, value = ... }` / `{ ok = false, error = ... }` in input order. Never rejects.
- `async.race(list)` → settles with the first input to settle, whether it resolves or rejects.
- `async.any(list)` → resolves with the first input to resolve. Rejects only when every input rejects.
- `async.timeout(promise, ms)` → resolves with `promise`'s value, or rejects with a timeout error if `promise` does not settle within `ms` milliseconds.
- `async.mapLimit(list, limit, fn)` → calls `fn(item)` (returning a promise) with at most `limit` in flight, and resolves to the results array in input order.

```lua
local async = require("async")

async.run(function()
    -- run three lookups in parallel and collect them in order.
    local results = async.all({ fetch("a"), fetch("b"), fetch("c") }):await()
    print(table.concat(results, ", "))

    -- bound concurrency to 2 at a time while preserving order.
    local doubled = async.mapLimit({ 1, 2, 3, 4 }, 2, function(n)
        return async.promise(function()
            async.sleep(5):await()
            return n * 2
        end)
    end):await()
    print(table.concat(doubled, ", "))
end)
```

## Promises

- `promise:await()` — pauses the current coroutine until the promise settles, then returns the value (or `nil, err` on failure).
- `promise:isDone()` → boolean. Treat it as a hint, not as synchronization.

See also the design notes in [../async.md](../async.md).

## Examples

### `promise_isdone.lua`

```lua
local async = require("async")

async.spawn(function()
    local p = async.sleep(5)
    assert(p:isDone() == false, "sleep promise should start pending")
    p:await()
    assert(p:isDone() == true, "sleep promise should be done after await")
    print("promise:isDone ok")
end)
```

### `sleep.lua`

```lua
local async = require("async")

async.spawn(function()
    local t0 = os.clock()
    async.sleep(50):await()
    local dt = (os.clock() - t0) * 1000
    print("async.sleep ok (requested 50ms, os.clock delta " .. string.format("%.1f", dt) .. " ms)")
end)
```

### `spawn.lua`

```lua
local async = require("async")

local done = false

async.spawn(function()
    assert(coroutine.isyieldable(), "spawned fn should run as coroutine")
    done = true
end)

async.spawn(function()
    async.sleep(1):await()
    assert(done, "inner spawn should have run")
    print("async.spawn ok")
end)
```

## Under the hood

Implemented directly on the runtime's event loop, with no external dependency.
