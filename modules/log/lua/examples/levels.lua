-- prints one line at each severity level
local log = require("log")

log.debug("debug", "line", 1)
log.info("info", "line", 2)
log.warn("warn", "line", 3)
log.error("error", "line", 4)
print("log levels emitted (check sink: spdlog/stdout/dummy per build)")
