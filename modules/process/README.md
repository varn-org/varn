# ⚙️ process

Run shell commands and read the process environment — `exec` runs off the main loop and resolves a
promise, while the environment, working-directory, and argument accessors are synchronous.

```lua
local async = require("async")
local process = require("process")

async.run(function()
    print(process.exec("echo varn"):await().stdout)
end)
```

## Capabilities

| Function | What it does |
|---|---|
| `process.exec(command, options?)` | Run `command` through the platform shell (`/bin/sh -c`, or `cmd.exe /c` on Windows); resolves a promise with `{stdout, stderr, code}`. Output is bounded at 64 MiB combined and a child that overruns the cap is killed; `code` is the exit status (`128 + signal` for a POSIX signal, so a cap overrun is `137` there). `options.timeoutMs` kills the child at the deadline and rejects the promise, which anything that can hang should carry since each call holds an io pool thread for its whole run. |
| `process.env` | Table of environment variable names to values, captured when the module is required. |
| `process.getenv(name, default?)` | The value of `name`, or `default` (or `nil`) when it is unset. |
| `process.cwd()` | The current working directory as a string. |
| `process.argv` | A 1-based array of the script arguments, dropping the script path and host options. |
| `process.available` | `true` when this build can run commands, `false` on builds with the stub driver. |

## Availability

Commands run via `fork`/`exec` (POSIX) or `CreateProcess` (Windows). Among Apple platforms only macOS
permits that, so iOS, tvOS, watchOS, and visionOS use the stub. The browser (wasm) has no process model
at all. On those targets `process.available` is `false` and `process.exec` rejects with
"not available in this build". See the [platform matrix](../../docs/platform-availability.md).

## Reference, examples, and tests

- Full reference: [docs/lua-api/process.md](../../docs/lua-api/process.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
