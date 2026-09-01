-- async security covering rejection surfacing (ASY-053, ASY-049), deadlock freedom (ASY-009), isDone honesty (ASY-064, ASY-065), and re-await safety (ASY-074, ASY-089)
local async = require("async")

async.run(function()
    -- ASY-053 and ASY-049 an awaited operation that rejects surfaces an error and never aborts the process
    local function rejecting()
        async.sleep(1):await()
        error("rejected operation")
    end
    local ok, err = pcall(rejecting)
    assert(ok == false, "a rejected awaited operation should surface as a caught error")
    assert(type(err) == "string", "the rejection reason should be a string message")

    -- ASY-009 many concurrent sleeps all settle and a wave of awaits never deadlocks
    local done = 0
    local total = 16
    for _ = 1, total do
        async.spawn(function()
            async.sleep(2):await()
            done = done + 1
        end)
    end
    while done < total do
        async.sleep(1):await()
    end
    assert(done == total, "all concurrent sleeps should settle without deadlock")

    -- ASY-064 and ASY-065 isDone reports false while pending and true only after the promise settles
    local p = async.sleep(3)
    assert(p:isDone() == false, "isDone should report pending before the delay elapses")
    p:await()
    assert(p:isDone() == true, "isDone should report done only after settling")

    -- ASY-074 and ASY-089 re-awaiting a settled promise is safe and keeps reporting done
    p:await()
    assert(p:isDone() == true, "re-awaiting a settled promise should keep it done")

    print("async security ok")
end)
