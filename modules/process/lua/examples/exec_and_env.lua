-- runs a command and prints its capture, environment, and working directory
local async = require("async")
local process = require("process")

async.run(function()
    local result, err = process.exec("echo varn && echo oops 1>&2"):await()
    assert(not err, err)

    print("code:   " .. result.code)
    print("stdout: " .. result.stdout)
    print("stderr: " .. result.stderr)

    print("home:   " .. process.getenv("HOME", "(unset)"))
    print("cwd:    " .. process.cwd())

    if #process.argv > 0 then
        print("argv:   " .. table.concat(process.argv, " "))
    end
end)
