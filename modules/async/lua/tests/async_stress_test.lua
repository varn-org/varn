-- concurrency and stress for the async runtime where many spawned coroutines, nested combinators, and heavy sleep churn all settle without hanging or leaking work on the event loop
local async = require("async")

async.run(function()
    -- fan out many independent coroutines and join them in order
    local n = 500
    local tasks = {}
    for i = 1, n do
        tasks[i] = async.promise(function()
            async.sleep(i % 5):await()
            return i
        end)
    end

    local results = async.all(tasks):await()
    assert(#results == n, "all should join every spawned task")
    local sum = 0
    for i = 1, n do
        assert(results[i] == i, "all should preserve order under load at index " .. i)
        sum = sum + results[i]
    end
    assert(sum == n * (n + 1) / 2, "every task result should be accounted for")

    -- nested combinators settle without deadlock, the fast branch winning the race
    local nested = async.race({
        async.all({ async.promise(function() return 1 end), async.promise(function() return 2 end) }),
        async.timeout(async.promise(function()
            async.sleep(50):await()
            return "slow"
        end), 5),
    }):await()
    assert(type(nested) == "table" and nested[1] == 1 and nested[2] == 2, "race should settle with the fast all branch")

    -- mapLimit under a tight limit over many items with interleaved sleeps stays bounded and complete
    local items = {}
    for i = 1, 300 do
        items[i] = i
    end

    local peak = 0
    local inFlight = 0
    local doubled = async.mapLimit(items, 4, function(x)
        return async.promise(function()
            inFlight = inFlight + 1
            if inFlight > peak then
                peak = inFlight
            end
            async.sleep(x % 3):await()
            inFlight = inFlight - 1
            return x * 2
        end)
    end):await()
    assert(#doubled == 300, "mapLimit should complete every item under stress")
    for i = 1, 300 do
        assert(doubled[i] == i * 2, "mapLimit stress should keep order at index " .. i)
    end
    assert(peak <= 4, "mapLimit stress should never exceed the limit")

    -- a spawned coroutine that errors is isolated and does not take down its siblings
    local settled = async.allSettled({
        async.promise(function() error("boom") end),
        async.promise(function() return "ok" end),
    }):await()
    assert(settled[1].ok == false, "a failing task should be reported as not ok")
    assert(settled[2].ok == true and settled[2].value == "ok", "sibling tasks should still succeed")

    print("async stress ok")
end)
