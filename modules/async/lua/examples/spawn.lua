local async = require("async")

local done = false

async.spawn(function()
    assert(coroutine.isyieldable(), "spawned fn should run as coroutine")
    done = true
end)

async.spawn(function()
    async.sleep(1):await()
    assert(done, "inner spawn should have run")
    print("async.spawn ok")
end)
