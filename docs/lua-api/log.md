# 📝 log

Leveled logging. `log.debug(...)`, `log.info(...)`, `log.warn(...)`, and `log.error(...)`
each take any number of values, like `print`: they are converted with `tostring`, joined by
tabs, and written as one line with the level tag.

## Structured fields

A trailing table argument is treated as structured fields and appended to the line as
space-separated `key=value` pairs instead of being printed inline.

```lua
local log = require("log")

-- renders: request done method=GET status=200 ms=12
log.info("request done", { method = "GET", status = 200, ms = 12 })
```

## `log.setLevel(level)`

Sets the runtime minimum level. `level` is one of `"debug"`, `"info"`, `"warn"`, or `"error"`.
Messages below the configured level are dropped. An unknown level raises an error.

```lua
log.setLevel("warn")
log.info("dropped")     -- below the floor, never written
log.warn("kept")        -- written
```

## `log.toFile(path[, rotating])`

Adds a file sink so logs are also written to `path` while keeping stdout. When `rotating` is
truthy the file rotates at five megabytes keeping five files; otherwise the file is appended to.

```lua
log.toFile("varn.log")          -- append to a single file
log.toFile("varn.log", true)    -- rotate at 5 MB, keep 5 files
```

## Examples

### `levels.lua`

```lua
-- prints one line at each severity level
local log = require("log")

log.debug("debug", "line", 1)
log.info("info", "line", 2)
log.warn("warn", "line", 3)
log.error("error", "line", 4)
print("log levels emitted")
```

### `config.lua`

```lua
-- configures the logger level, adds a file sink, and logs structured fields
local log = require("log")

log.setLevel("info")
log.toFile("varn.log")

log.info("request done", { method = "GET", path = "/health", status = 200, ms = 12 })
log.warn("slow response", { path = "/report", ms = 850 })
log.error("request failed", { path = "/order", status = 500 })
```

## Under the hood

The default sink uses spdlog, and the logging backend is selected at build time. `setLevel`
maps to spdlog's level filtering and `toFile` to its `basic_file_sink`/`rotating_file_sink`.
Messages are passed to spdlog as an argument, not as a format string, so `%`- and `{}`-style
content in a message is logged verbatim and never interpreted.
