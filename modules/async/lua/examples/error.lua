-- an error raised inside an awaited operation propagates and is catchable with pcall
local async = require("async")

async.spawn(function()
    local function failing()
        async.sleep(2):await()
        error("operation failed after the delay")
    end
    local ok, err = pcall(failing)
    print("async.error caught:", ok, err)
end)
