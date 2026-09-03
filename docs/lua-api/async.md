# ⏳ async

Coroutine-based concurrency over the event loop. Async functions return promises.

- `async.sleep(ms)` → a promise that resolves after `ms` milliseconds.
- `async.spawn(fn)` → run `fn` as a background coroutine that can `:await()` promises.
- `async.run(fn)` → run `fn` as the program's async entry point. The process exits when it returns, or with a non-zero status if an uncaught error escapes.
- `async.promise(fn)` → run `fn` as a coroutine and return a promise that resolves with its return value, or rejects with its error.
- `async.deferred()` → returns a promise plus a one-shot `resolve` function. Calling `resolve()` settles the promise from elsewhere on the event loop. If the `resolve` function is garbage-collected without ever being called, the still-pending promise is broken (rejected) so its awaiters resume with an error rather than hanging forever. The building block for connection pools, semaphores, and other event-driven waits.

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

### `allsettled.lua`

```lua
-- Collects per-input outcomes with async.allSettled, which never rejects.
local async = require("async")

local function resolveAfter(ms, value)
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

local function rejectAfter(ms, reason)
    return async.promise(function()
        async.sleep(ms):await()
        error(reason, 0)
    end)
end

async.run(function()
    local settled = async.allSettled({ resolveAfter(2, "ok"), rejectAfter(3, "nope") }):await()
    print("async.allSettled[1]:", settled[1].ok, settled[1].value)
    print("async.allSettled[2]:", settled[2].ok, settled[2].error)
    print("async.allSettled ok")
end)
```

### `any.lua`

```lua
-- Resolves with the first input to succeed with async.any, ignoring earlier rejections.
local async = require("async")

local function resolveAfter(ms, value)
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

local function rejectAfter(ms, reason)
    return async.promise(function()
        async.sleep(ms):await()
        error(reason, 0)
    end)
end

async.run(function()
    local won = async.any({ rejectAfter(2, "bad"), resolveAfter(4, "good") }):await()
    print("async.any winner:", won)
    print("async.any ok")
end)
```

### `await.lua`

```lua
-- Awaits several sleeps in sequence and prints the order they complete in.
local async = require("async")

async.spawn(function()
    local seq = {}
    for i = 1, 3 do
        async.sleep(2):await()
        seq[#seq + 1] = i
    end
    print("async.await sequence:", table.concat(seq, ","))
end)
```

### `combinators.lua`

```lua
-- Fetches several things in parallel with async.all and prints the combined result in order.
local async = require("async")

-- Simulates a slow lookup that resolves after ms milliseconds.
local function fetch(name, ms)
    return async.promise(function()
        async.sleep(ms):await()
        return name .. ":done"
    end)
end

async.run(function()
    local t0 = os.clock()

    -- The three lookups overlap and the total wait matches the slowest one.
    local results = async.all({
        fetch("alpha", 30),
        fetch("beta", 10),
        fetch("gamma", 20),
    }):await()

    local dt = (os.clock() - t0) * 1000
    print("async.all results:", table.concat(results, ", "))
    print(string.format("finished in about %.0f ms (parallel, not 60 ms serial)", dt))

    -- Race returns the quickest of several alternatives.
    local fastest = async.race({ fetch("slow", 40), fetch("fast", 5) }):await()
    print("async.race winner:", fastest)
end)
```

### `deferred.lua`

```lua
-- Creates a promise resolved from elsewhere with async.deferred, which returns a promise and a one-shot resolve function.
local async = require("async")

async.run(function()
    local promise, resolve = async.deferred()
    assert(promise:isDone() == false, "deferred promise should start pending")

    -- Another coroutine wakes the deferred promise by calling resolve.
    async.spawn(function()
        async.sleep(5):await()
        resolve()
    end)

    local value = promise:await()
    print("async.deferred resolved:", value)
    print("async.deferred ok")
end)
```

### `error.lua`

```lua
-- An error raised inside an awaited operation propagates and is catchable with pcall.
local async = require("async")

async.spawn(function()
    local function failing()
        async.sleep(2):await()
        error("operation failed after the delay")
    end
    local ok, err = pcall(failing)
    print("async.error caught:", ok, err)
end)
```

### `maplimit.lua`

```lua
-- Maps a list with at most limit promises in flight with async.mapLimit, preserving order.
local async = require("async")

async.run(function()
    local doubled = async.mapLimit({ 1, 2, 3, 4, 5, 6 }, 2, function(n)
        return async.promise(function()
            async.sleep(3):await()
            return n * 2
        end)
    end):await()
    print("async.mapLimit results:", table.concat(doubled, ", "))
    print("async.mapLimit ok")
end)
```

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

### `timeout.lua`

```lua
-- Bounds how long to wait on a promise with async.timeout.
local async = require("async")

local function resolveAfter(ms, value)
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

async.run(function()
    -- A promise that settles within the budget passes its value through.
    local quick = async.timeout(resolveAfter(2, "in time"), 50):await()
    print("async.timeout in time:", quick)

    -- A promise that misses the budget rejects with a timeout error.
    local value, err = async.timeout(resolveAfter(50, "too slow"), 3):await()
    print("async.timeout elapsed:", value, err)
    print("async.timeout ok")
end)
```
## Under the hood

Implemented directly on the runtime's event loop, with no external dependency.
