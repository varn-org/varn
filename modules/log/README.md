# 📝 log

Leveled logging with structured fields and optional file sinks — backed by spdlog.

```lua
local log = require("log")
log.info("request done", { method = "GET", status = 200, ms = 12 })
```

## Capabilities

| Function | What it does |
|---|---|
| `log.debug(...)` | Write one line at the debug level; values are `tostring`'d and tab-joined like `print`. |
| `log.info(...)` | Write one line at the info level. |
| `log.warn(...)` | Write one line at the warn level. |
| `log.error(...)` | Write one line at the error level. |
| structured fields | A trailing table argument is appended as space-separated `key=value` pairs instead of printed inline. |
| `log.setLevel(level)` | Set the runtime minimum level (`"debug"`, `"info"`, `"warn"`, `"error"`); lower lines are dropped, an unknown level raises. |
| `log.toFile(path[, rotating])` | Add a file sink alongside stdout; `rotating` truthy rotates at 5 MB keeping 5 files, otherwise appends. |

## Reference, examples, and tests

- Full reference: [docs/lua-api/log.md](../../docs/lua-api/log.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
