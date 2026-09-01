-- a command given a deadline is killed when it passes it, so an endless child cannot hold an io pool thread for the life of the process
local async = require("async")
local platform = require("platform")
local process = require("process")

if not process.available then
    print("process not available on this build, skipping")
    return
end

-- the shell forks for these rather than exec'ing, so the process the deadline must reach is a grandchild holding the inherited pipe ends
local function sleeper(seconds)
    if platform.os() == "windows" then
        return string.format("ping -n %d 127.0.0.1 > nul", seconds + 1)
    end

    return string.format("sleep %d; echo finished", seconds)
end

async.run(function()
    -- a command that finishes inside its deadline is unaffected
    local quick = process.exec("echo inside", { timeoutMs = 30000 }):await()
    assert(quick.code == 0, "a command within its deadline should succeed, got " .. quick.code)
    assert(quick.stdout:find("inside", 1, true), "a command within its deadline should return its output")

    -- a command that overruns is killed and the promise rejects rather than resolving with a partial result
    local started = os.time()
    local result, err = process.exec(sleeper(30), { timeoutMs = 300 }):await()
    local elapsed = os.time() - started

    assert(result == nil, "an overrunning command must not resolve")
    assert(err ~= nil, "an overrunning command must report an error")
    assert(err:find("timeout", 1, true), "the error should name the timeout, got " .. tostring(err))
    assert(elapsed < 20, "the deadline should end the wait promptly, took " .. elapsed .. "s")

    -- the shell is not the only thing to kill: whatever it forked survives it and keeps running unless the deadline reaches the whole group
    if platform.os() ~= "windows" then
        local _, orphanErr = process.exec("sleep 987; echo finished", { timeoutMs = 300 }):await()
        assert(orphanErr ~= nil, "the grandchild command should still hit the deadline")

        -- the bracket keeps the pattern from matching the shell that carries this very command line
        local survivors = process.exec('pgrep -f "sle[e]p 987"'):await()
        assert(survivors.code ~= 0, "the deadline must leave no orphan behind, found: " .. survivors.stdout)
    end

    -- with no timeout the call still behaves exactly as before
    local unbounded = process.exec("echo unbounded"):await()
    assert(unbounded.code == 0, "an untimed command should still run")
    assert(unbounded.stdout:find("unbounded", 1, true), "an untimed command should still return its output")

    -- a negative deadline is refused at the call rather than being treated as unbounded
    local ok = pcall(function()
        return process.exec("echo x", { timeoutMs = -1 })
    end)
    assert(not ok, "a negative timeout should be rejected")

    print("process timeout ok")
end)
