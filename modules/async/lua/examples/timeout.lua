-- bounds how long to wait on a promise with async.timeout
local async = require("async")

local function resolveAfter(ms, value)
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

async.run(function()
    -- a promise that settles within the budget passes its value through
    local quick = async.timeout(resolveAfter(2, "in time"), 50):await()
    print("async.timeout in time:", quick)

    -- a promise that misses the budget rejects with a timeout error
    local value, err = async.timeout(resolveAfter(50, "too slow"), 3):await()
    print("async.timeout elapsed:", value, err)
    print("async.timeout ok")
end)
