-- collects per-input outcomes with async.allSettled, which never rejects
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
    local settled = async.allSettled({ resolveAfter(2, "ok"), rejectAfter(3, "nope") }):await()
    print("async.allSettled[1]:", settled[1].ok, settled[1].value)
    print("async.allSettled[2]:", settled[2].ok, settled[2].error)
    print("async.allSettled ok")
end)
