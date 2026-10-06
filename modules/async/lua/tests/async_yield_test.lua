-- The function `async.yield` hands the loop to every other ready task and lets the loop serve its timers between yields.
local async = require("async")

async.run(function()
    -- A yield answers a promise that is still pending when it is made and settles once the loop takes its next pass.
    local promise = async.yield()
    assert(promise:isDone() == false, "A fresh yield should be pending.")
    assert(promise:await() == "ok", "A yield should resolve to \"ok\".")

    -- Two tasks that yield in turn interleave, since each yield lets the other task run before it resumes.
    local order = {}
    local left = 2
    for _, name in ipairs({ "a", "b" }) do
        async.spawn(function()
            for _ = 1, 3 do
                order[#order + 1] = name
                async.yield():await()
            end

            left = left - 1
        end)
    end

    while left > 0 do
        async.yield():await()
    end

    assert(table.concat(order) == "ababab", "Yielding tasks should interleave, not " .. table.concat(order) .. ".")

    -- A task that yields in a loop never starves a timer, which fires between its passes.
    local fired = false
    async.spawn(function()
        async.sleep(2):await()
        fired = true
    end)

    local spins = 0
    while not fired do
        async.yield():await()
        spins = spins + 1
    end

    assert(spins > 0, "The timer should fire only after the loop took more passes.")

    -- A promise that already settled resumes its waiter in the same pass, while a yield lets a ticking task run first.
    local ticking = true
    local ticks = 0
    async.spawn(function()
        while ticking do
            ticks = ticks + 1
            async.yield():await()
        end
    end)

    local settled, settle = async.deferred()
    settle()
    local before = ticks
    assert(settled:await() == "ok", "A settled deferred should answer \"ok\".")
    assert(ticks == before, "Awaiting a settled promise should not let another task run.")

    async.yield():await()
    assert(ticks > before, "A yield should let the ticking task run.")
    ticking = false

    print("The \"async.yield\" tests passed.")
end)
