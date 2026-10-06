-- Yields and errors mixed across every protected boundary, covering yields across `pcall` and `xpcall`, errors raised after yields, yields from metamethods and from `coroutine.wrap`, closing a suspended coroutine, and `await` inside nested `pcall`.
local async = require("async")

-- A coroutine yields across `pcall`, keeps every value, and the protected call answers once it ends.
do
    local co = coroutine.create(function(a)
        local ok, b, c = pcall(function()
            local x = coroutine.yield(a + 1)
            local y = coroutine.yield(x * 2)
            return x, y
        end)
        assert(ok and b == 10 and c == "y", 'The "pcall" around the yields should answer the values its function returned.')
        return "done"
    end)

    local _, first = coroutine.resume(co, 1)
    assert(first == 2, "The first yield should carry the argument plus one.")

    local _, second = coroutine.resume(co, 10)
    assert(second == 20, "The second yield should carry the resumed value twice.")

    local ok, last = coroutine.resume(co, "y")
    assert(ok and last == "done" and coroutine.status(co) == "dead", "The coroutine should finish after its last resume.")
end

-- A yield inside `xpcall` keeps the handler, which still runs for an error raised after the yield.
do
    local co = coroutine.wrap(function()
        return xpcall(function()
            coroutine.yield("paused")
            error("Late failure", 0)
        end, function(message)
            return "handled: " .. message
        end)
    end)

    assert(co() == "paused", 'The coroutine should pause inside "xpcall".')
    local ok, message = co()
    assert(ok == false and message == "handled: Late failure", 'The handler of "xpcall" should run for an error raised after a yield.')
end

-- An error raised after many yields reaches the resume with its value intact, and the coroutine is dead.
do
    local failure = { code = 42 }
    local co = coroutine.create(function()
        for i = 1, 100 do
            coroutine.yield(i)
        end

        error(failure)
    end)

    for i = 1, 100 do
        local ok, value = coroutine.resume(co)
        assert(ok and value == i, "Each resume should answer the next yielded value.")
    end

    local ok, err = coroutine.resume(co)
    assert(ok == false and err == failure, "The error object should reach the resume unchanged after the yields.")
    assert(coroutine.status(co) == "dead", "A coroutine that failed should be dead.")
end

-- An error caught by a `pcall` after a yield leaves the coroutine running, and a later yield still works.
do
    local co = coroutine.wrap(function()
        local ok, err = pcall(function()
            coroutine.yield(1)
            error("Inner", 0)
        end)
        assert(ok == false and err == "Inner", 'The inner "pcall" should catch the error raised after a yield.')
        coroutine.yield(2)
        return 3
    end)

    assert(co() == 1 and co() == 2 and co() == 3, "The coroutine should keep yielding after it caught an error.")
end

-- Metamethods written in Lua yield, and the operation that called them answers the resumed value.
do
    local meta = {
        __index = function(_, key)
            return coroutine.yield("index " .. key)
        end,
        __add = function(_, other)
            return coroutine.yield("add") + other
        end,
        __lt = function()
            return coroutine.yield("lt")
        end,
        __concat = function(_, other)
            return coroutine.yield("concat") .. other
        end,
        __call = function(_, value)
            return coroutine.yield("call") * value
        end,
        __eq = function()
            return coroutine.yield("eq")
        end,
    }
    local object = setmetatable({}, meta)
    local other = setmetatable({}, meta)

    local co = coroutine.wrap(function()
        local results = {}
        results[#results + 1] = object.field
        results[#results + 1] = object + 5
        results[#results + 1] = object < other
        results[#results + 1] = object .. "!"
        results[#results + 1] = object(3)
        results[#results + 1] = object == other
        return results
    end)

    assert(co() == "index field", 'The "__index" metamethod should yield.')
    assert(co("value") == "add", 'The "__add" metamethod should yield.')
    assert(co(10) == "lt", 'The "__lt" metamethod should yield.')
    assert(co(true) == "concat", 'The "__concat" metamethod should yield.')
    assert(co("text") == "call", 'The "__call" metamethod should yield.')
    assert(co(7) == "eq", 'The "__eq" metamethod should yield.')

    local results = co(false)
    assert(results[1] == "value" and results[2] == 15 and results[3] == true, "The operations should answer the values their metamethods resumed with.")
    assert(results[4] == "text!" and results[5] == 21 and results[6] == false, "The operations should answer the values their metamethods resumed with.")
end

-- A wrapped coroutine nested in another one yields to its own caller, and its error reaches the caller with the position it was raised at.
do
    local inner = coroutine.wrap(function()
        coroutine.yield("inner")
        error("Inner failure")
    end)

    local outer = coroutine.wrap(function()
        local value = inner()
        coroutine.yield("outer " .. value)
        local ok, err = pcall(inner)
        return ok, err
    end)

    assert(outer() == "outer inner", "The outer coroutine should see the yield of the inner one.")
    local ok, err = outer()
    assert(ok == false and err:find(":%d+: Inner failure$"), "The error of the wrapped coroutine should keep its position.")
end

-- A suspended coroutine closes its pending variables in reverse order when closed, and one closer that yields is resumed through.
do
    local closed = {}
    local co = coroutine.create(function()
        local first <close> = setmetatable({}, { __close = function() closed[#closed + 1] = "first" end })
        local second <close> = setmetatable({}, { __close = function() closed[#closed + 1] = "second" end })
        coroutine.yield()
    end)

    coroutine.resume(co)
    assert(coroutine.close(co) == true, "Closing a suspended coroutine should succeed.")
    assert(closed[1] == "second" and closed[2] == "first", "The pending variables should close in reverse order.")

    local co2 = coroutine.wrap(function()
        do
            local guard <close> = setmetatable({}, { __close = function() coroutine.yield("closing") end })
        end

        return "after"
    end)

    assert(co2() == "closing", 'A "__close" metamethod should yield.')
    assert(co2() == "after", "The scope should finish once the closer resumes.")
end

-- Many coroutines interleave their yields without disturbing one another.
do
    local threads = {}
    for i = 1, 50 do
        threads[i] = coroutine.wrap(function()
            local total = 0
            for step = 1, 20 do
                total = total + coroutine.yield(i * step)
            end

            return total
        end)
    end

    for i = 1, 50 do
        assert(threads[i]() == i, "The first yield should carry the identity of its coroutine.")
    end

    for step = 1, 19 do
        for i = 1, 50 do
            assert(threads[i](1) == i * (step + 1), "Each coroutine should keep its own state across interleaved yields.")
        end
    end

    for i = 1, 50 do
        assert(threads[i](1) == 20, "Each coroutine should total its own resumed values.")
    end
end

-- A yield outside a coroutine is refused with an error, and an error crossing a C call boundary keeps its message.
do
    local ok, err = pcall(coroutine.yield, 1)
    assert(ok == false and err:find("outside a coroutine"), "A yield outside a coroutine should raise.")

    local co = coroutine.wrap(function()
        return pcall(table.sort, { 3, 2, 1 }, function(a, b)
            coroutine.yield()
            return a < b
        end)
    end)

    local sorted, message = co()
    assert(sorted == false and message:find("yield across a C%-call boundary"), "A yield through a C function without a continuation should raise.")
end

async.run(function()
    -- An `await` inside nested `pcall` resumes inside both, and each level answers its own values.
    local outerOk, innerOk, value = pcall(function()
        local ok, result = pcall(function()
            async.sleep(1):await()
            local settled = async.promise(function()
                async.sleep(1):await()
                return "settled"
            end):await()
            return settled .. "!"
        end)
        return ok, result
    end)
    assert(outerOk and innerOk and value == "settled!", 'An "await" inside nested "pcall" should resume with its value.')

    -- An error raised after an `await` stops at the innermost `pcall` and leaves the outer one untouched.
    local reached = false
    local ok, innerResult, innerErr = pcall(function()
        local caughtOk, caughtErr = pcall(function()
            async.sleep(1):await()
            error({ reason = "after await" })
        end)
        reached = true
        return caughtOk, caughtErr
    end)
    assert(ok and reached and innerResult == false and innerErr.reason == "after await", 'The innermost "pcall" should catch an error raised after an "await".')

    -- A rejected promise awaited inside nested `pcall` answers its error without raising.
    local nestedOk, rejectedValue, rejectedErr = pcall(function()
        return select(2, pcall(function()
            return async.promise(function()
                async.sleep(1):await()
                error("Rejected", 0)
            end):await()
        end))
    end)
    assert(nestedOk and rejectedValue == nil and rejectedErr == "Rejected", 'A rejected "await" should answer its error inside nested "pcall".')

    -- An `await` inside `xpcall` keeps the handler for an error raised once it resumed.
    local handledOk, handled = xpcall(function()
        async.sleep(1):await()
        error("After the await", 0)
    end, function(message)
        return "handled: " .. message
    end)
    assert(handledOk == false and handled == "handled: After the await", 'The handler of "xpcall" should run for an error raised after an "await".')

    -- Many awaits in a loop under `pcall` keep the running total.
    local total = 0
    local loopOk = pcall(function()
        for i = 1, 200 do
            async.sleep(0):await()
            total = total + i
        end
    end)
    assert(loopOk and total == 20100, 'Every "await" in a loop under "pcall" should resume once.')
end)
