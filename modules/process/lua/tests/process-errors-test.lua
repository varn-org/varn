-- The options of "exec" are read strictly, the deadline holds after the child closes its output, and a working directory that cannot be read raises a sentence.
local async = require("async")
local fs = require("fs")
local platform = require("platform")
local process = require("process")

if not process.available then
    print("The \"process\" module is not available in this build, skipping.")
    return
end

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')

-- A deadline that is not a whole number of milliseconds is refused rather than read as no deadline at all.
local function refused(options)
    local ok, err = pcall(process.exec, "echo x", options)
    return not ok and tostring(err):find('The option "timeoutMs"', 1, true) ~= nil
end

assert(refused({ timeoutMs = 1.5 }), "A fractional deadline should be refused, naming the option")
assert(refused({ timeoutMs = "100" }), "A deadline given as a string should be refused, naming the option")
assert(refused({ timeoutMs = true }), "A deadline given as a boolean should be refused, naming the option")
assert(not pcall(process.exec, "echo x", 100), "Options that are not a table should be refused")

async.run(function()
    assert(process.exec("echo x", { timeoutMs = 30000 }):await().code == 0, "A whole deadline should be accepted")

    if platform.os() == "windows" then
        print("The \"process\" error tests passed.")
        return
    end

    -- A child that closes both streams and keeps running is still killed at its deadline.
    local started = os.time()
    local result, err = process.exec("exec >/dev/null 2>&1; sleep 30", { timeoutMs = 300 }):await()
    local elapsed = os.time() - started
    assert(result == nil and err and err:find("timeout", 1, true), "A child past its deadline should reject, got " .. tostring(err))
    assert(elapsed < 10, "The deadline should hold after the child closed its output, took " .. elapsed .. "s")

    -- A deadline shorter than the start of the shell still reaches the group the child leads.
    for _ = 1, 5 do
        local _, early = process.exec("sleep 30 & wait", { timeoutMs = 1 }):await()
        assert(early ~= nil, "A very short deadline should reject")
    end

    assert(os.time() - started < 20, "Very short deadlines should end their children promptly")

    -- A child whose working directory was removed reads it as an error with a sentence.
    local binary = assert(arg[-1], "The test needs the \"varn\" binary from \"arg[-1]\"")
    if binary:sub(1, 1) ~= "/" then
        binary = process.cwd() .. "/" .. binary
    end

    local script = dir .. "/process-errors-cwd.lua"
    fs.writeFile(script, 'local ok, err = pcall(require("process").cwd)\nprint(ok, err)\n'):await()

    local gone = dir .. "/process-errors-gone"
    local child = process.exec(string.format('mkdir "%s" && cd "%s" && rmdir "%s" && "%s" "%s"', gone, gone, gone, binary, script)):await()
    assert(child.code == 0, "The child should run, got " .. child.code .. ": " .. child.stdout .. child.stderr)
    assert(child.stdout:find("false\tThe current working directory could not be read.", 1, true), "The child should report the missing directory, got " .. child.stdout)

    print("The \"process\" error tests passed.")
end)
