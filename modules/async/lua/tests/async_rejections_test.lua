-- A promise that rejects with no one observing it reaches the failure handler once as a rejection, the uses that observe a promise keep it from being reported, and "catch" answers a promise of what its handler returns.
local async = require("async")
local fs = require("fs")
local platform = require("platform")
local process = require("process")

local dir = os.getenv("VARN_TEST_DIR") or "."

-- The handler takes only the rejections this test causes, and raises any other failure, such as a failed assertion of the test, so it fails the run instead of being taken.
local reports = {}
async.onFailure(function(err, traceback, frames, kind)
    if kind ~= "rejection" then
        error(err, 0)
    end

    reports[#reports + 1] = { err = err, traceback = traceback, frames = frames }
end)

local function failing(message)
    return async.promise(function()
        error(message, 0)
    end)
end

-- The `cmd` shell drops the outer quote pair of a `/c` string, so a Windows command needs one more around the whole thing.
local function shellCommand(program, argument)
    local quoted = string.format('"%s" "%s"', program, argument)
    if platform.os() == "windows" then
        return '"' .. quoted .. '"'
    end

    return quoted
end

async.run(function()
    -- A task that fails with no one observing its promise is reported with the error as raised, its traceback and its frames.
    local reason = { code = "lost" }
    async.promise(function()
        error(reason)
    end)
    async.sleep(10):await()
    assert(#reports == 1, "An unobserved rejection should be reported once, not " .. #reports .. " times.")
    assert(rawequal(reports[1].err, reason), "The handler should receive the table the task raised.")
    assert(type(reports[1].traceback) == "string" and reports[1].traceback:find("stack traceback", 1, true), "The handler should receive the traceback of the task.")
    assert(reports[1].frames[1].name == "error", "The innermost frame should be the call that raised.")
    assert(reports[1].frames[2].source:find("async_rejections_test.lua", 1, true) and reports[1].frames[2].line == 39, "The next frame should be the line that raised, not " .. tostring(reports[1].frames[2].line) .. ".")

    -- A task that fails after an await is reported too.
    async.promise(function()
        async.sleep(1):await()
        error("late", 0)
    end)
    async.sleep(20):await()
    assert(#reports == 2 and reports[2].err == "late", "A rejection after an await should be reported.")

    -- A promise native code rejects is reported with its message alone.
    fs.readFile(dir .. "/missing/file.txt")
    async.sleep(50):await()
    assert(#reports == 3 and type(reports[3].err) == "string" and reports[3].err:find("FsStorage", 1, true), "A native rejection should be reported with its message.")
    assert(reports[3].traceback == nil and reports[3].frames == nil, "A native rejection carries no traceback and no frames.")

    -- An await observes a promise, whether it comes after the rejection or before it.
    local _, early = failing("early"):await()
    assert(early == "early", "An await should answer the error.")
    local pending = async.promise(function()
        async.sleep(1):await()
        error("pending", 0)
    end)
    local _, late = pending:await()
    assert(late == "pending", "An await of a pending promise should answer its error.")

    -- A catch observes a promise and answers a promise of what its handler returns, or of the value of a promise that resolved.
    local recovered = failing("broken"):catch(function(err)
        return "recovered " .. err
    end)
    assert(recovered:await() == "recovered broken", "A catch should resolve with what its handler returns.")
    local passed = async.promise(function()
        return 5
    end):catch(function()
        return 0
    end)
    assert(passed:await() == 5, "A catch of a promise that resolves should resolve with its value.")
    local ok, refused = pcall(function()
        return passed:catch("not a function")
    end)
    assert(not ok and refused:find('"catch"', 1, true), "A catch should refuse a handler that is not a function, not: " .. tostring(refused))

    -- Every combinator observes the promises it is given, and a catch observes the promise of the combinator.
    local quiet = function() end
    async.all({ failing("all") }):catch(quiet)
    async.allSettled({ failing("allSettled") }):await()
    async.race({ failing("race") }):catch(quiet)
    async.any({ failing("any") }):catch(quiet)
    async.timeout(failing("timeout"), 1000):catch(quiet)
    async.mapLimit({ 1 }, 1, function()
        return failing("mapLimit")
    end):catch(quiet)
    async.sleep(30):await()

    -- The resolver of a deferred that was dropped breaks its promise, which no one observes and nothing reports, since nothing failed.
    do
        local _, _ = async.deferred()
    end
    collectgarbage()
    collectgarbage()
    async.sleep(10):await()
    assert(#reports == 3, "The observed and broken promises should not be reported, but " .. tostring(reports[4] and reports[4].err) .. " was.")

    -- The check "isDone" does not observe a promise, since it never reads the error.
    local polled = failing("polled")
    assert(polled:isDone(), 'The check "isDone" should see the rejection.')
    async.sleep(10):await()
    assert(#reports == 4 and reports[4].err == "polled", 'A promise only checked with "isDone" should be reported.')

    -- A promise reported once is never reported again, and an await after the report still answers its error.
    local _, after = polled:await()
    async.sleep(10):await()
    assert(after == "polled" and #reports == 4, "A reported promise should answer a later await without a second report.")

    -- A catch whose handler fails answers a promise that rejects, which is reported when no one observes it.
    failing("first"):catch(function()
        error("second", 0)
    end)
    async.sleep(10):await()
    assert(#reports == 5 and reports[5].err == "second", "The failure of the handler of a catch should be reported.")

    -- Without a handler the runtime logs the rejection as unobserved and stops the loop with a failure.
    if process.available then
        local binary = arg[-1]
        assert(binary, "The test needs the \"varn\" binary from \"arg[-1]\"")

        local childPath = dir .. "/unobserved_child.lua"
        fs.writeFile(childPath, table.concat({
            'local async = require("async")',
            'async.promise(function() error("Nobody waited for this.", 0) end)',
            'async.sleep(10000)',
        }, "\n")):await()

        local result = process.exec(shellCommand(binary, childPath)):await()
        local output = result.stdout .. result.stderr
        assert(result.code == 1, "The child should exit with a failure, got " .. result.code .. " and output: " .. output)
        assert(output:find("A promise rejected and nothing observed it. Nobody waited for this.", 1, true), "The child should log the unobserved rejection, output: " .. output)
    end

    async.onFailure(nil)
    print("The \"async\" rejection tests passed.")
end)
