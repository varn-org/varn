-- Exercises the `nil` path of `process.getenv` and the `exec` of a command the shell cannot find.
local async = require("async")
local process = require("process")

-- The `getenv` call without a default returns `nil` for a missing variable.
assert(process.getenv("VARN_DEFINITELY_MISSING_VARIABLE_XYZ") == nil, 'A missing "getenv" without a default returns "nil"')

async.run(function()
    -- A command that does not exist exits non-zero rather than raising.
    local result = process.exec("varn_no_such_command_xyz_12345"):await()
    assert(type(result) == "table" and result.code ~= 0, "A non-existent command should exit non-zero")
    print("The \"process\" extra tests passed.")
end)
