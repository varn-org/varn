-- A script that fails after `listen` leaves its accept watcher armed, and that handler holds Lua registry refs, so the runtime must release it while the state is still open instead of crashing on the way out.
local async = require("async")
local fs = require("fs")
local platform = require("platform")
local process = require("process")

if not process.available then
    print("The \"process\" module is not available in this build, skipping.")
    return
end

-- The `cmd` shell drops the outer quote pair of a `/c` string, so a Windows command needs one more around the whole thing.
local function shellCommand(program, argument)
    local quoted = string.format('"%s" "%s"', program, argument)
    if platform.os() == "windows" then
        return '"' .. quoted .. '"'
    end

    return quoted
end

local binary = arg[-1]
assert(binary, "The test needs the \"varn\" binary from \"arg[-1]\"")

local dir = os.getenv("VARN_TEST_DIR") or "."
local childPath = dir .. "/listen_then_fail_child.lua"

async.run(function()
    fs.writeFile(childPath, table.concat({
        'local http = require("http")',
        'local app = http.createApp()',
        'app:get("/ok", function(ctx) ctx:text("alive") end)',
        'app:listen({ host = "127.0.0.1", port = 39838 })',
        'error("Deliberate failure after listen")',
    }, "\n")):await()

    local result = process.exec(shellCommand(binary, childPath)):await()
    local output = result.stdout .. result.stderr

    -- A signalled child is reported as 128 plus the signal, so a segmentation fault arrives as 139.
    assert(result.code == 1, "The child should exit with the script error, got " .. result.code .. " and output: " .. output)
    assert(not output:find("Fatal signal", 1, true), "The child must not crash while tearing down, output: " .. output)
    assert(output:find("Deliberate failure after listen", 1, true), "The child should report the script error, output: " .. output)

    print("The \"http\" listen then fail tests passed.")
end)
