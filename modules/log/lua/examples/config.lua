-- configures the logger level adds a file sink and logs structured fields
local log = require("log")

-- drop anything below info for this run
log.setLevel("info")

-- this line is below the floor so it is dropped
log.debug("starting up")

-- also write every line to a file alongside stdout
log.toFile("varn.log")

-- a trailing table becomes space-separated key=value fields
log.info("request done", { method = "GET", path = "/health", status = 200, ms = 12 })
log.warn("slow response", { path = "/report", ms = 850 })
log.error("request failed", { path = "/order", status = 500 })

print("log config example emitted (see varn.log)")
