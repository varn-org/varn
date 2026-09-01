-- adds a rotating file sink that rolls at 5 MB keeping 5 files
local log = require("log")

log.toFile("varn-rotating.log", true)
log.info("rotating sink configured")
print("log to_file_rotating example ok")
