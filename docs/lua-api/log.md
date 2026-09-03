# 📝 log

Leveled logging. `log.debug(...)`, `log.info(...)`, `log.warn(...)`, and `log.error(...)` each take any number of values, like `print`: they are converted with `tostring`, joined by tabs, and written as one line with the level tag.

## Structured fields

A trailing table argument is treated as structured fields and appended to the line as space-separated `key=value` pairs instead of being printed inline.

```lua
local log = require("log")

-- renders: request done method=GET status=200 ms=12
log.info("request done", { method = "GET", status = 200, ms = 12 })
```

## `log.setLevel(level)`

Sets the runtime minimum level. `level` is one of `"debug"`, `"info"`, `"warn"`, or `"error"`. Messages below the configured level are dropped. An unknown level raises an error.

```lua
log.setLevel("warn")
log.info("dropped")     -- below the floor, never written
log.warn("kept")        -- written
```

## `log.toFile(path[, rotating])`

Adds a file sink so logs are also written to `path` while keeping stdout. When `rotating` is truthy the file rotates at five megabytes keeping five files. Otherwise the file is appended to.

```lua
log.toFile("varn.log")          -- append to a single file
log.toFile("varn.log", true)    -- rotate at 5 MB, keep 5 files
```

## Examples

### Config

```lua
-- Configures the logger level adds a file sink and logs structured fields.
local log = require("log")

-- Drop anything below info for this run.
log.setLevel("info")

-- This line is below the floor so it is dropped.
log.debug("starting up")

-- Also write every line to a file alongside stdout.
log.toFile("varn.log")

-- A trailing table becomes space-separated key=value fields.
log.info("request done", { method = "GET", path = "/health", status = 200, ms = 12 })
log.warn("slow response", { path = "/report", ms = 850 })
log.error("request failed", { path = "/order", status = 500 })

print("log config example emitted (see varn.log)")
```

### Debug

```lua
-- Emits a single line at the debug level.
local log = require("log")

log.debug("debug example", "value", 1)
print("log debug example ok")
```

### Error

```lua
-- Emits a single line at the error level.
local log = require("log")

log.error("error example", "value", 4)
print("log error example ok")
```

### Info

```lua
-- Emits a single line at the info level.
local log = require("log")

log.info("info example", "value", 2)
print("log info example ok")
```

### Levels

```lua
-- Prints one line at each severity level.
local log = require("log")

log.debug("debug", "line", 1)
log.info("info", "line", 2)
log.warn("warn", "line", 3)
log.error("error", "line", 4)
print("log levels emitted (check sink: spdlog/stdout/dummy per build)")
```

### Setting the level

```lua
-- Raises the minimum level so lines below the floor are dropped.
local log = require("log")

log.setLevel("warn")
log.info("dropped")
log.warn("kept")
print("log set_level example ok")
```

### Structured

```lua
-- A trailing table is appended as space-separated key=value fields.
local log = require("log")

log.info("request done", { method = "GET", status = 200, ms = 12 })
print("log structured example ok")
```

### Rotating file sink

```lua
-- Adds a rotating file sink that rolls at 5 MB keeping 5 files.
local log = require("log")

log.toFile("varn-rotating.log", true)
log.info("rotating sink configured")
print("log to_file_rotating example ok")
```

### Warn

```lua
-- Emits a single line at the warn level.
local log = require("log")

log.warn("warn example", "value", 3)
print("log warn example ok")
```
## Under the hood

The default sink uses spdlog, and the logging backend is selected at build time. `setLevel` maps to spdlog's level filtering and `toFile` to its `basic_file_sink`/`rotating_file_sink`. Messages are passed to spdlog as an argument, not as a format string, so `%`- and `{}`-style content in a message is logged verbatim and never interpreted.
