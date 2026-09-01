local async = require("async")

async.spawn(function()
    local t0 = os.clock()
    async.sleep(50):await()
    local dt = (os.clock() - t0) * 1000
    print("async.sleep ok (requested 50ms, os.clock delta " .. string.format("%.1f", dt) .. " ms)")
end)
