-- async features exercising sleep, spawn, promise await and isDone, sequencing, ordering, and errors
local async = require("async")

async.run(function()
    -- await returns only after the delay and isDone reports true once settled
    local promise = async.sleep(3)
    assert(promise:isDone() == false, "a fresh sleep promise should be pending")
    promise:await()
    assert(promise:isDone() == true, "the sleep promise should be done after await")

    -- awaiting the same settled promise again is safe and stays done
    promise:await()
    assert(promise:isDone() == true, "re-awaiting a settled promise should keep it done")

    -- running several awaits in sequence completes in the order they were issued
    local seq = {}
    for i = 1, 3 do
        async.sleep(1):await()
        seq[#seq + 1] = i
    end
    assert(table.concat(seq, ",") == "1,2,3", "sequential awaits should complete in issue order")

    -- spawn runs a background coroutine that can await, coordinated via a shared counter
    local done = 0
    local total = 5
    for _ = 1, total do
        async.spawn(function()
            async.sleep(1):await()
            done = done + 1
        end)
    end
    while done < total do
        async.sleep(1):await()
    end
    assert(done == total, "all spawned coroutines should complete")

    -- distinct delays complete in delay order regardless of registration order
    local order = {}
    async.spawn(function()
        async.sleep(6):await()
        order[#order + 1] = "c"
    end)
    async.spawn(function()
        async.sleep(2):await()
        order[#order + 1] = "a"
    end)
    async.spawn(function()
        async.sleep(4):await()
        order[#order + 1] = "b"
    end)
    while #order < 3 do
        async.sleep(1):await()
    end
    assert(table.concat(order, ",") == "a,b,c", "shorter delays should finish first")

    -- deferred returns a pending promise plus a one-shot resolve function that settles it from elsewhere
    local deferredPromise, resolveDeferred = async.deferred()
    assert(type(resolveDeferred) == "function", "deferred should return a resolve function")
    assert(deferredPromise:isDone() == false, "a fresh deferred promise should be pending")
    async.spawn(function()
        async.sleep(1):await()
        resolveDeferred()
    end)
    local deferredValue = deferredPromise:await()
    assert(deferredValue == "ok", "awaiting a resolved deferred promise should return its value")
    assert(deferredPromise:isDone() == true, "a resolved deferred promise should report done")

    -- an error raised inside an awaited helper propagates and is catchable with pcall
    local function failing()
        async.sleep(1):await()
        error("boom from an awaited operation")
    end
    local ok, err = pcall(failing)
    assert(ok == false, "the awaited failure should be caught by pcall")
    assert(type(err) == "string" and err:find("boom"), "the error reason should reach the caller")

    print("async features ok")
end)
