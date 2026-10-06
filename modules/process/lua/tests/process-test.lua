-- Runs a successful and a failing command and probes env and cwd.
local async = require("async")
local process = require("process")

if not process.available then
    print("The \"process\" module is not available in this build, skipping.")
    return
end

async.run(function()
    local ok, okErr = process.exec("echo hello"):await()
    assert(not okErr, okErr)
    assert(ok.code == 0, 'The command "echo" should exit zero')
    assert(ok.stdout:find("hello", 1, true), 'The field "stdout" should contain "hello"')

    local fail, failErr = process.exec("exit 3"):await()
    assert(not failErr, failErr)
    assert(fail.code == 3, "A failing command should report its nonzero code")

    assert(type(process.env) == "table", 'The field "env" should be a table')
    assert(type(process.getenv("PATH")) == "string", 'The variable "PATH" should be present')
    assert(process.getenv("VARN_DOES_NOT_EXIST", "fallback") == "fallback", "A missing variable should use the default")

    local cwd = process.cwd()
    assert(type(cwd) == "string" and #cwd > 0, 'The call "cwd" should return a non-empty string')

    assert(type(process.argv) == "table", 'The field "argv" should be a table')

    print("The \"process\" tests passed.")
end)
