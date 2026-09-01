-- resolves with the first input to succeed with async.any, ignoring earlier rejections
local async = require("async")

local function resolveAfter(ms, value)
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

local function rejectAfter(ms, reason)
    return async.promise(function()
        async.sleep(ms):await()
        error(reason, 0)
    end)
end

async.run(function()
    local won = async.any({ rejectAfter(2, "bad"), resolveAfter(4, "good") }):await()
    print("async.any winner:", won)
    print("async.any ok")
end)
