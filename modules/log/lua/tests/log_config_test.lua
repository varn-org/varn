-- Exercises the `setLevel` filtering, structured fields and the file sink round-trip.
local log = require("log")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set. Run the tests with "python3 varn.py test".')

-- The configuration surface exists as functions.
assert(type(log.setLevel) == "function", 'The field "setLevel" should be a function')
assert(type(log.toFile) == "function", 'The field "toFile" should be a function')

-- The `setLevel` call accepts every documented level without erroring.
for _, level in ipairs({ "debug", "info", "warn", "error" }) do
    assert(pcall(log.setLevel, level), 'The level "' .. level .. '" should be valid')
end

-- An unknown level is rejected.
assert(not pcall(log.setLevel, "verbose"), "An unknown level should error")

-- Raising the floor to `error` drops lower levels without crashing.
log.setLevel("error")
assert(pcall(log.debug, "Suppressed debug"), 'A "debug" call below the floor should not crash')
assert(pcall(log.info, "Suppressed info"), 'An "info" call below the floor should not crash')
assert(pcall(log.warn, "Suppressed warn"), 'A "warn" call below the floor should not crash')

-- Structured fields render as space-separated `key=value` pairs.
log.setLevel("debug")
assert(pcall(log.info, "Request done", { method = "GET", status = 200, ms = 12 }), "Structured fields should not crash")
assert(pcall(log.error, "Failed", { code = 500 }), 'An "error" call with fields should not crash')

-- The file sink writes content that can be read back.
local path = dir .. "/log_config_test.log"
os.remove(path)
log.toFile(path)

local marker = "varn-log-file-marker-" .. tostring(os.time())
log.info(marker, { sink = "file", n = 7 })

local file = assert(io.open(path, "rb"), 'The log file should exist after "toFile"')
local content = file:read("*a")
file:close()

assert(content:find(marker, 1, true), "The logged marker should be present in the file")
assert(content:find("sink=file", 1, true), "The structured field should be present in the file")
assert(content:find("n=7", 1, true), "The numeric structured field should be present in the file")

os.remove(path)

-- The rotating variant also writes readable content.
local rpath = dir .. "/log_config_rotating_test.log"
os.remove(rpath)
log.toFile(rpath, true)

local rmarker = "varn-log-rotating-marker-" .. tostring(os.time())
log.info(rmarker, { sink = "rotating" })

local rfile = assert(io.open(rpath, "rb"), 'The rotating log file should exist after "toFile"')
local rcontent = rfile:read("*a")
rfile:close()

assert(rcontent:find(rmarker, 1, true), "The logged marker should be present in the rotating file")
assert(rcontent:find("sink=rotating", 1, true), "The structured field should be present in the rotating file")

os.remove(rpath)

print("The \"log\" config tests passed.")
