-- Task handles that cancel a task at its next await and close its to-be-closed variables, awaits that resume only the coroutine still waiting in them, and the one handler that receives every task failure.
local async = require("async")

async.run(function()
    -- A cancelled task is never resumed by what it awaits.
    local reached = false
    local task = async.spawn(function()
        async.sleep(20):await()
        reached = true
    end)
    assert(type(task) == "table" and type(task.cancel) == "function", 'The handle "async.spawn" answers should cancel the task.')
    task.cancel()
    async.sleep(60):await()
    assert(not reached, "A cancelled task should never pass its await.")

    -- A task cancelled while it runs stops at its next await, even one that would answer at once.
    local ready, finishReady = async.deferred()
    finishReady()
    local after = false
    local own
    own = async.spawn(function()
        async.sleep(5):await()
        own.cancel()
        ready:await()
        after = true
    end)
    async.sleep(40):await()
    assert(not after, "A task that cancelled itself should stop at its next await.")

    -- A cancelled task closes its to-be-closed variables, at once when it is suspended and right after its next await when it cancelled itself.
    local closed = {}
    local function guard(name)
        return setmetatable({}, { __close = function() closed[#closed + 1] = name end })
    end

    local suspended = async.spawn(function()
        local held <close> = guard("suspended")
        async.sleep(10000):await()
    end)
    suspended.cancel()
    assert(closed[1] == "suspended", "A suspended task should close its variables as it is cancelled.")

    local running
    running = async.spawn(function()
        local held <close> = guard("running")
        async.sleep(5):await()
        running.cancel()
        async.sleep(10000):await()
    end)
    async.sleep(40):await()
    assert(closed[2] == "running", "A task that cancelled itself should close its variables at its next await.")

    -- A coroutine that left an await is not resumed when that promise settles later, and the await it is in answers it.
    local first, finishFirst = async.deferred()
    local second, finishSecond = async.deferred()
    local answered = nil
    local worker = coroutine.create(function()
        first:await()
        answered = select(1, second:await())
    end)
    coroutine.resume(worker)
    coroutine.resume(worker, "moved on")
    finishFirst()
    async.sleep(10):await()
    assert(answered == nil, "A promise should not resume a coroutine that left its await.")
    finishSecond()
    async.sleep(10):await()
    assert(answered == "ok", "The await the coroutine is in should resume it.")

    -- A failing task reaches the handler with its error and a traceback, a table error intact, and the loop goes on.
    -- The handler takes only the failures this test causes, and raises any other one, such as a failed assertion of the test, so it fails the run instead of being taken.
    local failures = {}
    local expected = { ["late failure"] = true, ["too deep"] = true, ["the guard broke"] = true }
    async.onFailure(function(err, traceback, frames)
        if type(err) ~= "table" and not expected[err] then
            error(err, 0)
        end

        failures[#failures + 1] = { err = err, traceback = traceback, frames = frames }
    end)

    async.spawn(function()
        error({ code = "broken" }, 0)
    end)
    async.spawn(function()
        async.sleep(1):await()
        error("late failure", 0)
    end)
    async.sleep(20):await()
    assert(#failures == 2, "Every failing task should reach the handler.")
    assert(type(failures[1].err) == "table" and failures[1].err.code == "broken", "A table error should reach the handler intact.")
    assert(failures[2].err == "late failure", "A failure after an await should reach the handler.")
    assert(type(failures[2].traceback) == "string" and failures[2].traceback:find("stack traceback", 1, true), "The handler should receive a traceback.")

    -- The frames name where the failure was raised, from the innermost out, so no host reads the traceback back.
    local frames = failures[2].frames
    assert(type(frames) == "table" and #frames > 0, "The handler should receive the frames of the failure.")
    assert(frames[1].kind == "C" and frames[1].name == "error", "The innermost frame should be the call that raised.")
    assert(frames[1].namewhat == "global", "A frame should say how its function was named.")
    local own = frames[2]
    assert(own.kind == "Lua" and own.source:find("async_tasks_test.lua", 1, true) and math.type(own.line) == "integer", "The next frame should be the task, with its source and line.")
    assert(math.type(own.linedefined) == "integer" and own.linedefined > 0, "A frame of Lua should say where its function was defined.")

    -- A runaway recursion keeps its innermost and its outermost frames around a marker of the frames left out, so where it started still shows.
    local function dive(depth)
        if depth == 0 then
            error("too deep", 0)
        end

        return (dive(depth - 1))
    end

    async.spawn(function()
        dive(500)
    end)
    async.sleep(10):await()
    local deep = failures[3].frames
    local marker
    for index, frame in ipairs(deep) do
        if frame.kind == "skipped" then
            marker = index
        end
    end
    assert(marker ~= nil and math.type(deep[marker].count) == "integer" and deep[marker].count > 400, "A deep stack should mark the frames it left out.")
    assert(deep[#deep].kind == "C" or deep[#deep].source:find("async_tasks_test.lua", 1, true), "The outermost frames should stay after the marker.")

    -- A failure of a closing variable of a cancelled task reaches the handler, whether it raised a text or a table.
    local failing = async.spawn(function()
        local held <close> = setmetatable({}, { __close = function() error("the guard broke", 0) end })
        async.sleep(10000):await()
    end)
    failing.cancel()
    assert(failures[4] ~= nil and failures[4].err == "the guard broke", "A text a closing variable raised should reach the handler.")

    local failingTable = async.spawn(function()
        local held <close> = setmetatable({}, { __close = function() error({ code = "guard" }) end })
        async.sleep(10000):await()
    end)
    failingTable.cancel()
    assert(failures[5] ~= nil and type(failures[5].err) == "table" and failures[5].err.code == "guard", "A table a closing variable raised should reach the handler intact.")

    async.onFailure(nil)
    print("The \"async\" task tests passed.")
end)
