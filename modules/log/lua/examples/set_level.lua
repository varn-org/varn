-- raises the minimum level so lines below the floor are dropped
local log = require("log")

log.setLevel("warn")
log.info("dropped")
log.warn("kept")
print("log set_level example ok")
