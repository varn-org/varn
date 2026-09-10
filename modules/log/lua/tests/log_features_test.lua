-- Exercises the documented surface across every level.
local log = require("log")

-- The four documented severity helpers are all functions.
for _, level in ipairs({ "debug", "info", "warn", "error" }) do
    assert(type(log[level]) == "function", 'The field "' .. level .. '" should be a function')
end

-- Each level is callable with a plain message.
assert(pcall(log.debug, "Debug message"), 'The "debug" call should not error')
assert(pcall(log.info, "Info message"), 'The "info" call should not error')
assert(pcall(log.warn, "Warn message"), 'The "warn" call should not error')
assert(pcall(log.error, "Error message"), 'The "error" call should not error')

-- A non-string argument is handled like `print`.
assert(pcall(log.info, 123), "A number argument should be handled")
assert(pcall(log.info, true), "A boolean argument should be handled")
assert(pcall(log.info, nil), 'A "nil" argument should be handled')
assert(pcall(log.info), "No arguments should be handled")

-- Multiple calls in sequence all succeed.
for i = 1, 5 do
    assert(pcall(log.info, "Sequence", i), "A sequential call should not error")
end

print("The \"log\" features tests passed.")
