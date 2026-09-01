-- fetches several things in parallel with async.all and prints the combined result in order
local async = require("async")

-- simulates a slow lookup that resolves after ms milliseconds
local function fetch(name, ms)
    return async.promise(function()
        async.sleep(ms):await()
        return name .. ":done"
    end)
end

async.run(function()
    local t0 = os.clock()

    -- the three lookups overlap and the total wait matches the slowest one
    local results = async.all({
        fetch("alpha", 30),
        fetch("beta", 10),
        fetch("gamma", 20),
    }):await()

    local dt = (os.clock() - t0) * 1000
    print("async.all results:", table.concat(results, ", "))
    print(string.format("finished in about %.0f ms (parallel, not 60 ms serial)", dt))

    -- race returns the quickest of several alternatives
    local fastest = async.race({ fetch("slow", 40), fetch("fast", 5) }):await()
    print("async.race winner:", fastest)
end)
