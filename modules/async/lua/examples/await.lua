-- awaits several sleeps in sequence and prints the order they complete in
local async = require("async")

async.spawn(function()
    local seq = {}
    for i = 1, 3 do
        async.sleep(2):await()
        seq[#seq + 1] = i
    end
    print("async.await sequence:", table.concat(seq, ","))
end)
