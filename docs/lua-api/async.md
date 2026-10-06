# ⏳ async

Coroutine-based concurrency over the event loop. Async functions return promises.

- `async.sleep(ms)` → A promise that resolves after `ms` milliseconds.
- `async.yield()` → A promise that resolves to `"ok"` in the next pass of the loop, so a task that awaits it lets every other ready task, timer and socket take its turn first. It is the way a long loop of work gives the loop back, and the way it lets a host that polls with a budget end its frame.
- `async.spawn(fn)` → Run `fn` as a background coroutine that can `:await()` promises, and answer its task handle.
- `async.run(fn)` → Run `fn` as the program's async entry point and answer its task handle. The process exits when it returns, or with a non-zero status if an uncaught error escapes.
- `task.cancel()` → Stop a task for good at its next await. A cancelled task is never resumed by what it awaits, including a promise that has already settled, and its coroutine is closed, which runs its to-be-closed variables, at once when it is suspended and right after its next await when the task cancels itself. A failure a closing variable raises reaches the handler of `async.onFailure`.
- `async.onFailure(handler)` → Set the one handler that receives `handler(err, traceback, frames, kind)` for every failure no caller receives: a task of `spawn` or `run` that fails, a handler of `http.createServer`, a route of `http.createApp`, a WebSocket callback or a callback of `http.client.stream` that fails, an ffi callback that fails outside any ffi call, and a promise that rejects with no one observing it. The value `err` is what was raised, a table included, `traceback` is where it was raised as text and `frames` is the same place as a list from the innermost call out, each frame a table with `source`, `line`, `name`, `namewhat` (how the function was named, such as `"global"` or `"method"`), `linedefined` (where a Lua function starts) and `kind` (`"Lua"`, `"C"` or `"main"`), where a field is absent when the call has none. The value `kind` is `"rejection"` for a promise no one observed and `"failure"` for everything else. A stack deeper than the list keeps its innermost and its outermost frames around one frame of kind `"skipped"` whose `count` says how many were left out, so a runaway recursion still shows where it started. A failure a closing variable of a cancelled task raised arrives with its error alone, since its stack is gone. The traceback and the frames are both `nil` for a callback called from another thread and for a promise that native code rejected, such as a failed read of `fs`. A failure the handler receives is handled, so the loop goes on, while a handler that fails itself leaves the failure to the runtime. With no handler, or one that fails, the runtime logs a failed task and an unobserved rejection and stops the loop, and logs the failure of a server handler or a callback and goes on. The handler runs at once and cannot await, so it starts a task of its own for work that waits. Passing `nil` clears it.
- `async.promise(fn)` → Run `fn` as a coroutine and return a promise that resolves with its return value, or rejects with its error. A rejection no one observes reaches `async.onFailure` with the error as raised, its traceback and its frames.
- `async.deferred()` → Returns a promise plus a one-shot `resolve` function. Calling `resolve()` settles the promise from elsewhere on the event loop. If the `resolve` function is garbage-collected without ever being called, the still-pending promise is broken (rejected) so its awaiters resume with an error rather than hanging forever. Nothing failed, so a broken promise no one observes is never reported. The building block for connection pools, semaphores, and other event-driven waits.

## Passes of the loop

The loop runs in passes. A pass runs the jobs that were queued and the timers that were due when it started, then serves every socket that is ready, so a timer armed during a pass, even for zero milliseconds, and a task resumed by a promise settled during a pass wait for the next pass. An `:await()` of a promise that has already settled answers at once without yielding, so its task keeps running in the same pass, while an await that waits resumes in a later pass once the promise settles. A program run with `async.run` takes every pass there is until it ends, and a host that polls with a budget takes passes while they make progress and its budget lasts, so `async.sleep(0)` and `async.yield()` both hand that host its frame back.

## Combinators

Each combinator returns a promise you can `:await()`. A combinator waits on its inputs without polling, so the loop sleeps until one of them settles.

- `async.all(list)` → Resolves to an array of every promise's result in input order. Rejects as soon as any input rejects.
- `async.allSettled(list)` → Resolves to an array of `{ ok = true, value = ... }` / `{ ok = false, error = ... }` in input order. Never rejects.
- `async.race(list)` → Settles with the first input to settle, whether it resolves or rejects.
- `async.any(list)` → Resolves with the first input to resolve. Rejects only when every input rejects.
- `async.timeout(promise, ms)` → Resolves with `promise`'s value, or rejects with a timeout error if `promise` does not settle within `ms` milliseconds. A promise that settles first removes the timer of the deadline, so a deadline that was met keeps the loop neither busy nor alive.
- `async.mapLimit(list, limit, fn)` → Calls `fn(item)` (returning a promise) with at most `limit` in flight, and resolves to the results array in input order.

```lua
local async = require("async")

async.run(function()
    -- Run three lookups in parallel and collect them in order.
    local results = async.all({ fetch("a"), fetch("b"), fetch("c") }):await()
    print(table.concat(results, ", "))

    -- Bound concurrency to 2 at a time while preserving order.
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

- `promise:await()` — Pauses the current coroutine until the promise settles, then returns the value (or `nil, err` on failure). A promise resumes only a coroutine still suspended in its await, so a coroutine that moved on is never resumed with an answer it no longer waits for. Every awaiter receives the same value, before the promise settles or after it, so a large string a native call settled it with, such as the content `fs.readFile` read, exists once in Lua however many tasks await it.
- `promise:catch(fn)` → Observe the promise and answer a new promise that resolves with what `fn(err)` returns when the promise rejects, or with the value of the promise when it resolves. A handler that fails rejects the new promise, which is observed or reported like any other. A handler that is not a function is refused, and the promise stays unobserved.
- `promise:isDone()` → A boolean. Treat it as a hint, not as synchronization. It does not observe the promise, since it never reads the error.

See also the design notes in [`../async.md`](../async.md).

## Unobserved rejections

A promise that rejects with no one observing it hands its error to `async.onFailure` as a rejection, once, the way a browser reports an unhandled rejection. The check runs in the job the rejection posts to the loop, which runs once the code that was running when the promise rejected returned to the loop, so that code, such as the rest of the task that called `async.promise`, can still observe it before it is reported. A promise observed later still answers its error, and is never reported a second time.

- These uses observe a promise: `promise:await()`, whether the promise is pending or settled and even in a cancelled task, `promise:catch(fn)`, and passing it to `async.all`, `async.allSettled`, `async.race`, `async.any`, `async.timeout` or, as the result of its mapper, `async.mapLimit`. A combinator observes its inputs at once, even the ones it no longer needs once it settled, and its own promise is observed or reported like any other.
- These do not observe a promise: `promise:isDone()`, keeping it in a variable, and dropping it.
- Work whose failure the caller means to ignore takes `promise:catch(function() end)`.
- Nothing is reported for a deferred broken by its dropped resolver, or for a stream of `http.client.stream` that rejects with the failure of its callback, since that failure already reached the handler.

## Examples

### All settled

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

### Any

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

### Await

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

### Combinators

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

### Deferred

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

### Error

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

### Map with a concurrency limit

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

### Catch

```lua
-- Recovers from a rejection with catch, and ignores one that does not matter.
local async = require("async")

async.run(function()
    local recovered = async.promise(function()
        error("not found", 0)
    end):catch(function(err)
        return "default after " .. err
    end):await()
    print("async.catch:", recovered)

    -- A rejection nothing observes reaches async.onFailure, so work whose failure does not matter catches it.
    async.promise(function()
        error("ignored", 0)
    end):catch(function() end)
    print("async.catch ok")
end)
```

### Checking whether a promise settled

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

### Yield

```lua
-- Splits a long computation into slices with async.yield, so a timer and other tasks keep running between the slices.
local async = require("async")

async.run(function()
    local ticks = 0
    async.spawn(function()
        for _ = 1, 3 do
            async.sleep(1):await()
            ticks = ticks + 1
        end
    end)

    local sum = 0
    for i = 1, 3000000 do
        sum = sum + i
        if i % 1000 == 0 then
            async.yield():await()
        end
    end

    print("async.yield sum:", sum, "ticks while summing:", ticks)
end)
```

### Sleep

```lua
local async = require("async")

async.spawn(function()
    local t0 = os.clock()
    async.sleep(50):await()
    local dt = (os.clock() - t0) * 1000
    print("async.sleep ok (requested 50ms, os.clock delta " .. string.format("%.1f", dt) .. " ms)")
end)
```

### Spawn

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

### Tasks

```lua
local async = require("async")

-- Every failing task and every rejection no one observes reach one handler with the error and a traceback, and the loop goes on.
async.onFailure(function(err, traceback, frames, kind)
    print("a " .. kind .. " reached the handler:", err)
    print(traceback)
end)

async.run(function()
    local task = async.spawn(function()
        async.sleep(1000):await()
        print("never printed")
    end)

    -- The cancelled task stops at its await for good.
    task.cancel()

    async.spawn(function()
        error("broken", 0)
    end)

    -- Nothing awaits this promise, so its rejection is reported once the turn that rejected it ends.
    async.promise(function()
        error("forgotten", 0)
    end)

    async.sleep(10):await()
    print("async tasks ok")
end)
```

### Timeout

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
