-- argument-type error paths for the async entry points and the sleep non-positive clamp
local async = require("async")

-- spawn, run and promise reject a non-function argument
assert(not pcall(function() return async.spawn(42) end), "spawn should reject a non-function")
assert(not pcall(function() return async.run(42) end), "run should reject a non-function")
assert(not pcall(function() return async.promise("x") end), "promise should reject a non-function")

-- sleep rejects a non-number delay
assert(not pcall(function() return async.sleep("soon") end), "sleep should reject a non-number delay")

-- a non-positive sleep clamps to zero and still resolves promptly
async.run(function()
    async.sleep(0):await()
    async.sleep(-5):await()
    print("async errors ok")
end)
