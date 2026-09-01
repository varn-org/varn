-- process edge cases covering many concurrent executions and large or interleaved output that a serial pipe drain would deadlock on
local async = require("async")
local process = require("process")
local platform = require("platform")

if not process.available then
    print("process not available on this build, skipping")
    return
end

async.run(function()
    -- many executions run at once and each returns only its own output
    local promises = {}
    for i = 1, 16 do
        promises[i] = process.exec("echo item" .. i)
    end
    local results = async.all(promises):await()
    for i = 1, 16 do
        assert(results[i].code == 0, "concurrent exec " .. i .. " should exit zero")
        assert(results[i].stdout:find("item" .. i, 1, true), "concurrent exec " .. i .. " should return its own output")
    end

    -- the large-output cases assume a posix shell, so they run only where that shell backs process.exec
    if platform.os() ~= "windows" then
        -- a stdout payload well past the 64k pipe buffer comes back intact
        local big = process.exec("head -c 300000 /dev/zero"):await()
        assert(big.code == 0, "the large stdout command should exit zero")
        assert(#big.stdout == 300000, "stdout should return the full payload, got " .. #big.stdout)

        -- a large stderr payload comes back intact on its own channel
        local err = process.exec("head -c 300000 /dev/zero 1>&2"):await()
        assert(#err.stderr == 300000, "stderr should return the full payload, got " .. #err.stderr)

        -- a child that floods stderr while the parent still expects stdout must not deadlock the drain
        local deadlock = process.exec("head -c 300000 /dev/zero 1>&2; printf done"):await()
        assert(#deadlock.stderr == 300000, "the flooded stderr should be complete, got " .. #deadlock.stderr)
        assert(deadlock.stdout == "done", "stdout should still arrive after the stderr flood, got " .. deadlock.stdout)

        -- stdout and stderr are captured independently
        local split = process.exec("echo out; echo err 1>&2"):await()
        assert(split.stdout:find("out", 1, true), "stdout should carry the stdout line")
        assert(split.stderr:find("err", 1, true), "stderr should carry the stderr line")
        assert(not split.stdout:find("err", 1, true), "stdout should not carry the stderr line")
    end

    print("process features ok")
end)
