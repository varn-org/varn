# ⏳ Concurrency and async

This page explains how Varn runs your code and the rules to follow so you stay responsive instead of blocking.

## 🧵 One line of execution

Your script, your HTTP handlers, timers, and promise results all run together, one at a time. This keeps things simple and predictable.

The trade-off: if you do something slow directly, like a long loop or a blocking read, nothing else runs until it finishes. The fix is to move slow work to the background and get the result back through a promise.

## 🎟️ Promises and `:await()`

Anything that does slow work returns a promise instead of blocking. Call `:await()` to pause until it finishes and get the value:

```lua
local fs = require("fs")
local async = require("async")

async.spawn(function()
  local text, err = fs.readFile("notes.txt"):await()
  if err then
    print("could not read:", err)
    return
  end
  print(text)
end)
```

`:await()` pauses the current coroutine and lets everything else keep running. When the work finishes, your coroutine resumes right after the `:await()`.

`:await()` only works inside a coroutine. Varn opens one for you in `async.run(fn)`, `async.spawn(fn)`, and inside an HTTP handler. At the top level of a plain script there is no coroutine to pause, so call `async.run(fn)` and put your code inside `fn`. A promise settles exactly once, either resolved or rejected. Later settles are ignored.

## 📦 The async module

- `async.sleep(ms)` returns a promise that resolves after `ms` milliseconds without blocking.
- `async.spawn(fn)` runs `fn` as a background coroutine so it can `:await()` promises. Any error that escapes `fn` ends the process with a non-zero exit code.
- `async.run(fn)` runs `fn` as the program's async entry point. The process exits cleanly when `fn` returns, or with a non-zero status if an uncaught error escapes. Use it at the top level of a script instead of `async.spawn` when you want the script to end with `fn`.
- `async.promise(fn)` runs `fn` as a coroutine and returns a promise that resolves with its return value or rejects with its error. Use it to turn an async helper into something the rest of your code can `:await()`.

Combinators build on these primitives and each return a promise: `async.all`, `async.allSettled`, `async.race`, `async.any`, `async.timeout`, and `async.mapLimit`. See [lua-api/async.md](lua-api/async.md) for their exact contracts.

```lua
local async = require("async")

async.spawn(function()
  print("start")
  async.sleep(500):await() -- everything else stays free during this pause
  print("half a second later")
end)

print("this line runs first, before the sleep finishes")
```

## ✅ Do and ❌ Don't

Do:

- Use `:await()` for I/O (files, network, sockets, zip). It keeps the server responsive.
- Wrap independent work in `async.spawn` so the pieces can interleave.
- Read the second return of `:await()` for errors, as in `local value, err = p:await()`.

Don't:

- Don't run heavy CPU work or a busy loop directly. It blocks every other request until it ends. Keep handlers short, or break the work up.
- Don't fake waiting with `os.execute` or a manual busy-loop. Use `async.sleep` or `:await()`.
- Don't call `:await()` at the top level of a script. Wrap your code in `async.run` (one-shot scripts) or `async.spawn` (background tasks).

## 🌍 In the browser

The same code runs, but anything that needs a listening socket or native crypto is unavailable, and outbound HTTP is routed through the browser. `async.sleep` and promises behave the same as elsewhere. Treat `:isDone()` as a hint, never as a lock.
