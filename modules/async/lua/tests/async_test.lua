-- async run executes as a coroutine, sleep yields, and promises report completion
local async = require("async")

async.run(function()
    assert(coroutine.isyieldable(), "the entry should run as a coroutine")

    local promise = async.sleep(5)
    assert(promise:isDone() == false, "promise should start pending")
    promise:await()
    assert(promise:isDone() == true, "promise should be done after await")

    print("async ok")
end)
