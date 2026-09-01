-- creates a promise resolved from elsewhere with async.deferred, which returns a promise and a one-shot resolve function
local async = require("async")

async.run(function()
    local promise, resolve = async.deferred()
    assert(promise:isDone() == false, "deferred promise should start pending")

    -- another coroutine wakes the deferred promise by calling resolve
    async.spawn(function()
        async.sleep(5):await()
        resolve()
    end)

    local value = promise:await()
    print("async.deferred resolved:", value)
    print("async.deferred ok")
end)
