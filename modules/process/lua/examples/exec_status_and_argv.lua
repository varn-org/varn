-- runs a successful and a failing command and prints env, cwd, and argv
local async = require("async")
local process = require("process")

if not process.available then
    print("process not available")
    return
end

async.run(function()
    local ok = process.exec("echo hello"):await()
    print("ok code:   " .. ok.code)
    print("ok stdout: " .. ok.stdout)

    local fail = process.exec("exit 3"):await()
    print("fail code: " .. fail.code)

    print("path:      " .. process.getenv("PATH", "(unset)"))
    print("missing:   " .. process.getenv("VARN_NOPE", "(default)"))
    print("env type:  " .. type(process.env))
    print("cwd:       " .. process.cwd())
    print("argv:      " .. table.concat(process.argv, " "))

    print("process status and argv ok")
end)
