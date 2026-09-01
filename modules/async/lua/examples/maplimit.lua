-- maps a list with at most limit promises in flight with async.mapLimit, preserving order
local async = require("async")

async.run(function()
    local doubled = async.mapLimit({ 1, 2, 3, 4, 5, 6 }, 2, function(n)
        return async.promise(function()
            async.sleep(3):await()
            return n * 2
        end)
    end):await()
    print("async.mapLimit results:", table.concat(doubled, ", "))
    print("async.mapLimit ok")
end)
