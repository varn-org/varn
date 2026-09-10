-- Async security covering rejection surfacing (ASY-053, ASY-049), deadlock freedom (ASY-009), `isDone` honesty (ASY-064, ASY-065), and re-await safety (ASY-074, ASY-089).
local async = require("async")

async.run(function()
    -- For ASY-053 and ASY-049, an awaited operation that rejects surfaces an error and never aborts the process.
    local function rejecting()
        async.sleep(1):await()
        error("Rejected operation")
    end
    local ok, err = pcall(rejecting)
    assert(ok == false, "A rejected awaited operation should surface as a caught error.")
    assert(type(err) == "string", "The rejection reason should be a string message.")

    -- For ASY-009, many concurrent sleeps all settle and a wave of awaits never deadlocks.
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
    assert(done == total, "All concurrent sleeps should settle without deadlock.")

    -- For ASY-064 and ASY-065, `isDone` reports false while pending and true only after the promise settles.
    local p = async.sleep(3)
    assert(p:isDone() == false, 'The "isDone" call should report pending before the delay elapses.')
    p:await()
    assert(p:isDone() == true, 'The "isDone" call should report done only after settling.')

    -- For ASY-074 and ASY-089, re-awaiting a settled promise is safe and keeps reporting done.
    p:await()
    assert(p:isDone() == true, "Re-awaiting a settled promise should keep it done.")

    print('The "async" security tests passed.')
end)
