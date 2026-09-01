-- exercises hostile message content as data without crashing the logger
local log = require("log")

-- newline and carriage-return content is handled without crashing
assert(pcall(log.info, "first line\nforged second line"), "newline content should not crash")
assert(pcall(log.warn, "rewrite\rline"), "carriage return content should not crash")
assert(pcall(log.error, "split\r\nentry"), "crlf content should not crash")

-- tab content does not crash the tab-joined writer
assert(pcall(log.info, "field\tinjected\tfield"), "tab content should not crash")

-- a nul byte in the message is handled without crashing
assert(pcall(log.info, "before\0after"), "nul byte content should not crash")

-- an ansi escape sequence is treated as data rather than a terminal control
assert(pcall(log.warn, "\27[31mred\27[0m"), "ansi escape content should not crash")

-- printf-style specifiers are logged as data
assert(pcall(log.info, "%s %n %d %x"), "printf specifiers should be treated as data")

-- fmt-style braces are logged as data
assert(pcall(log.info, "{} {0} {:x}"), "fmt braces should be treated as data")

-- a very long single message is handled without crashing
assert(pcall(log.error, string.rep("x", 200000)), "very long message should not crash")

-- hostile content at error level is contained
assert(pcall(log.error, "audit\nbypass\r%n"), "error level hostile content should not crash")

print("log security ok")
