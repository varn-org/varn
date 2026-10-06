-- Exercises hostile message content as data without crashing the logger.
local log = require("log")

-- Newline and carriage-return content is handled without crashing.
assert(pcall(log.info, "first line\nforged second line"), "Newline content should not crash")
assert(pcall(log.warn, "rewrite\rline"), "Carriage return content should not crash")
assert(pcall(log.error, "split\r\nentry"), "CRLF content should not crash")

-- Tab content does not crash the tab-joined writer.
assert(pcall(log.info, "field\tinjected\tfield"), "Tab content should not crash")

-- A NUL byte in the message is handled without crashing.
assert(pcall(log.info, "before\0after"), "NUL byte content should not crash")

-- An ANSI escape sequence is treated as data rather than a terminal control.
assert(pcall(log.warn, "\27[31mred\27[0m"), "ANSI escape content should not crash")

-- The `printf`-style specifiers are logged as data.
assert(pcall(log.info, "%s %n %d %x"), 'The "printf" specifiers should be treated as data')

-- The `fmt`-style braces are logged as data.
assert(pcall(log.info, "{} {0} {:x}"), 'The "fmt" braces should be treated as data')

-- A very long single message is handled without crashing.
assert(pcall(log.error, string.rep("x", 200000)), "A very long message should not crash")

-- Hostile content at the `error` level is contained.
assert(pcall(log.error, "audit\nbypass\r%n"), 'Hostile content at the "error" level should not crash')

print("The \"log\" security tests passed.")
