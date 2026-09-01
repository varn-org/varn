-- exercises the documented surface across every level
local log = require("log")

-- the four documented severity helpers are all functions
for _, level in ipairs({ "debug", "info", "warn", "error" }) do
    assert(type(log[level]) == "function", level .. " should be a function")
end

-- each level is callable with a plain message
assert(pcall(log.debug, "debug message"), "debug should not error")
assert(pcall(log.info, "info message"), "info should not error")
assert(pcall(log.warn, "warn message"), "warn should not error")
assert(pcall(log.error, "error message"), "error should not error")

-- a non-string argument is handled like print
assert(pcall(log.info, 123), "number argument should be handled")
assert(pcall(log.info, true), "boolean argument should be handled")
assert(pcall(log.info, nil), "nil argument should be handled")
assert(pcall(log.info), "no arguments should be handled")

-- multiple calls in sequence all succeed
for i = 1, 5 do
    assert(pcall(log.info, "sequence", i), "sequential call should not error")
end

print("log features ok")
