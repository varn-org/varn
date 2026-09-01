-- leveled logging goes to the host log rather than to print, so in an app these lines land in logcat or the device console instead of the panel below
local log = require("log")

log.info("Log", "starting the sample")
log.debug("Log", "a debug line")
log.warn("Log", "something looks odd")
log.error("Log", "and something failed")

print("the four lines above went to the host log; only print reaches this console")
