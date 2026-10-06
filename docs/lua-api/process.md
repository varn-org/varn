# ⚙️ process

Run commands and read the process environment. The function `exec` runs off the main loop and returns a promise. The environment and working-directory accessors are synchronous.

- `process.exec(command, options?)` → A promise resolving to a table `{stdout, stderr, code}`. The command runs through the platform shell — `/bin/sh -c` on POSIX and `cmd.exe /d /s /c` on Windows, which hands the command over exactly as written, quotes included — so shell features like pipes and redirection work with each shell's own syntax. Both streams are captured binary-safe up to a combined 64 MiB cap, past which the child is killed so an endless producer cannot exhaust host memory. The field `code` is the exit status. A child killed by a POSIX signal reports `128 + signal` (a cap overrun is `137`), while a terminated Windows process reports its own termination code.
- The option `options.timeoutMs` bounds how long the command may run, in whole milliseconds. The deadline covers the whole run, including a child that closed its output and keeps running. When it passes the deadline the child and everything it started are killed and the promise **rejects** rather than resolving with a partial result, so an overrun can never be mistaken for a normal exit. Without it the call waits as long as the child lives, matching `child_process.exec` in Node and `subprocess.run` in Python. A negative value, a fraction or a value that is not a number is refused at the call, naming the option.

  Each `exec` occupies one thread of the I/O pool for its whole run, and that pool also serves `fs` and the HTTP client. A command that may hang — anything reaching the network, or a tool that can wait on a lock or a prompt — should carry a `timeoutMs`, or enough of them will starve the pool the rest of the runtime depends on.

  ```lua
  local result, err = process.exec("curl -s https://example.com", { timeoutMs = 5000 }):await()
  if err then
      -- The command overran its deadline and was killed
  end
  ```
- `process.environment()` → A table mapping each environment variable name to its value at the time of the call. It raises `The environment of the process is not available in this build.` in the browser, which has no process environment.
- `process.getenv(name, default?)` → The value of `name`, or `default` (or `nil`) when it is unset. It raises the same error as `process.environment` in the browser.
- `process.cwd()` → The current working directory as a string. It raises an error when the directory cannot be read, such as one that was deleted, and in the browser.
- `process.argv` → An array of the script arguments. This is the same data as the global `arg` table, normalized to a 1-based array that drops `arg[0]` (the script path) and the host options, leaving just the arguments your script received.
- `process.available` → Holds `true` when this build can run commands. It holds `false` on iOS, tvOS, watchOS and visionOS, which let no app start another process, so `process.exec` rejects with `This system does not let an app start another process.` while the environment and the working directory stay readable, and in the browser, where `process.exec` rejects with `"not available in this build"`.

Commands are not sandboxed. Confine untrusted input before passing it here. A command containing a null byte is rejected rather than silently truncated. On builds without process support (such as the browser build) `process.exec` rejects with `"not available in this build"`, and the environment and the working directory raise, since those builds have no process environment to read.

## Examples

### Exec and the environment

```lua
-- Runs a command and prints its capture, environment, and working directory.
local async = require("async")
local process = require("process")

async.run(function()
    local result, err = process.exec("echo varn && echo oops 1>&2"):await()
    assert(not err, err)

    print("code:   " .. result.code)
    print("stdout: " .. result.stdout)
    print("stderr: " .. result.stderr)

    print("home:   " .. process.getenv("HOME", "(unset)"))
    print("cwd:    " .. process.cwd())

    if #process.argv > 0 then
        print("argv:   " .. table.concat(process.argv, " "))
    end
end)
```

### Exit status and argv

```lua
-- Runs a successful and a failing command and prints env, cwd, and argv.
local async = require("async")
local process = require("process")

if not process.available then
    print("process not available")
    return
end

async.run(function()
    local ok = process.exec("echo hello"):await()
    print("ok code:   " .. ok.code)
    print("ok stdout: " .. ok.stdout)

    local fail = process.exec("exit 3"):await()
    print("fail code: " .. fail.code)

    print("path:      " .. process.getenv("PATH", "(unset)"))
    print("missing:   " .. process.getenv("VARN_NOPE", "(default)"))
    print("env type:  " .. type(process.environment()))
    print("cwd:       " .. process.cwd())
    print("argv:      " .. table.concat(process.argv, " "))

    print("process status and argv ok")
end)
```
## Under the hood

On POSIX builds commands run through `fork` + `execl("/bin/sh", ...)` in a process group of their own, with pipes for `stdout` and `stderr`, capturing each stream until end of file. On Windows they run through `CreateProcess` inside a job object, so the deadline and the output cap terminate the whole tree. The browser build uses a stub driver that rejects `exec`.
