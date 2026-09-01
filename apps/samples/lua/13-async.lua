-- timers, background work and combinators all run on one event loop
local async = require("async")

local function delayed(value, ms)
    -- a combinator takes promises, and async.promise turns a coroutine into one
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

async.run(function()
    print("start")

    async.sleep(100):await()
    print("after 100ms")

    local results = async.all({ delayed("fast", 50), delayed("slow", 150) }):await()
    print("all:", results[1], results[2])

    print("race:", async.race({ delayed("winner", 10), delayed("loser", 200) }):await())

    local settled = async.allSettled({
        delayed("fine", 10),
        async.promise(function()
            error("this one fails")
        end),
    }):await()
    print("settled:", settled[1].ok, settled[1].value, "/", settled[2].ok)

    local doubled = async.mapLimit({ 1, 2, 3, 4 }, 2, function(n)
        return delayed(n * 2, 10)
    end):await()
    print("mapLimit:", table.concat(doubled, ", "))
end)
