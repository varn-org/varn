-- A log file that cannot be opened raises a sentence that gives the reason, rather than an error that holds only the path.
local log = require("log")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set. Run the tests with "python3 varn.py test".')

-- A regular file in the place of a folder makes the path impossible to create on every platform.
local blocker = dir .. "/log-errors-blocker"
local file = assert(io.open(blocker, "wb"))
file:write("x")
file:close()

for _, rotating in ipairs({ false, true }) do
    local ok, err = pcall(log.toFile, blocker .. "/inside.log", rotating)
    assert(not ok, "A log file under a regular file should be refused")
    assert(tostring(err):find("The log file could not be opened", 1, true), "The error should say what failed, got " .. tostring(err))
end

print("The \"log\" error tests passed.")
