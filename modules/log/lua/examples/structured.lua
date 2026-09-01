-- a trailing table is appended as space-separated key=value fields
local log = require("log")

log.info("request done", { method = "GET", status = 200, ms = 12 })
print("log structured example ok")
