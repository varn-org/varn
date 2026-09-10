-- Coroutines yield values back to the caller and resume where they stopped.
local function counter(limit)
    return coroutine.create(function()
        for i = 1, limit do
            coroutine.yield(i * i)
        end
        return "done"
    end)
end

local co = counter(4)
while true do
    local ok, value = coroutine.resume(co)
    print(coroutine.status(co), ok, value)
    if coroutine.status(co) == "dead" then
        break
    end
end
