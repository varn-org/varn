-- Leveled logging goes to the host log rather than to `print`, so in an app these lines land in Logcat or the device console instead of the panel below.
local log = require("log")

log.info("Log", "Starting the sample.")
log.debug("Log", "A debug line.")
log.warn("Log", "Something looks odd.")
log.error("Log", "Something failed.")

print('The four lines above went to the host log. Only "print" reaches this console.')
