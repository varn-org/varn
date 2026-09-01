-- runs a successful and a failing command and probes env and cwd
local async = require("async")
local process = require("process")

if not process.available then
    print("process not available on this build, skipping")
    return
end

async.run(function()
    local ok, okErr = process.exec("echo hello"):await()
    assert(not okErr, okErr)
    assert(ok.code == 0, "echo should exit zero")
    assert(ok.stdout:find("hello", 1, true), "stdout should contain hello")

    local fail, failErr = process.exec("exit 3"):await()
    assert(not failErr, failErr)
    assert(fail.code == 3, "failing command should report its nonzero code")

    assert(type(process.env) == "table", "env should be a table")
    assert(type(process.getenv("PATH")) == "string", "PATH should be present")
    assert(process.getenv("VARN_DOES_NOT_EXIST", "fallback") == "fallback", "missing var should use default")

    local cwd = process.cwd()
    assert(type(cwd) == "string" and #cwd > 0, "cwd should be a non-empty string")

    assert(type(process.argv) == "table", "argv should be a table")

    print("process ok")
end)
