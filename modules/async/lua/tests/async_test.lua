-- The `async.run` call executes its entry as a coroutine, `sleep` yields, and promises report completion.
local async = require("async")

async.run(function()
    assert(coroutine.isyieldable(), "The entry should run as a coroutine.")

    local promise = async.sleep(5)
    assert(promise:isDone() == false, "The promise should start pending.")
    promise:await()
    assert(promise:isDone() == true, "The promise should be done after the await.")

    print('The "async" tests passed.')
end)
