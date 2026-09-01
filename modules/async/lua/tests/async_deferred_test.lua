-- deferred promises covering one-shot resolve, idempotent resolve, many awaiters, the broken-promise break when the resolver is dropped, and that a held resolver keeps the promise pending
local async = require("async")

async.run(function()
    -- a resolved deferred settles its awaiter with the resolve value
    do
        local promise, resolve = async.deferred()
        local value
        async.spawn(function()
            value = promise:await()
        end)
        resolve()
        async.sleep(5):await()
        assert(value == "ok", "a resolved deferred should settle its awaiter with ok")
        assert(promise:isDone() == true, "a resolved deferred should report done")
    end

    -- calling the resolver more than once is a safe no-op
    do
        local promise, resolve = async.deferred()
        resolve()
        resolve()
        assert(promise:await() == "ok", "a second resolve call should not change the settled value")
    end

    -- every awaiter of a deferred is resumed when it resolves
    do
        local promise, resolve = async.deferred()
        local resumed = 0
        for _ = 1, 5 do
            async.spawn(function()
                promise:await()
                resumed = resumed + 1
            end)
        end
        resolve()
        while resumed < 5 do
            async.sleep(1):await()
        end
        assert(resumed == 5, "all awaiters should resume when the deferred resolves")
    end

    -- a resolver dropped before resolving breaks its promise so the awaiter is resumed with an error instead of hanging
    do
        local resumed = 0
        local broken = 0
        local function abandon()
            -- keep only the promise so the resolve function is unreferenced and collectable
            local promise = (async.deferred())
            async.spawn(function()
                local value, err = promise:await()
                resumed = resumed + 1
                if value == nil and err and tostring(err):find("discarded") then
                    broken = broken + 1
                end
            end)
        end

        for _ = 1, 20 do
            abandon()
        end
        collectgarbage("collect")
        collectgarbage("collect")

        while resumed < 20 do
            async.sleep(1):await()
        end
        assert(resumed == 20, "every awaiter of an abandoned deferred should be resumed")
        assert(broken == 20, "an abandoned deferred should break its awaiters with a broken-promise error")
    end

    -- holding the resolver keeps the promise pending, so the break happens only on a real drop
    do
        local promise, resolve = async.deferred()
        local settled = false
        async.spawn(function()
            promise:await()
            settled = true
        end)
        collectgarbage("collect")
        collectgarbage("collect")
        async.sleep(10):await()
        assert(settled == false, "a deferred with a live resolver should stay pending")
        assert(promise:isDone() == false, "a live-resolver deferred should not be broken")

        resolve()
        async.sleep(5):await()
        assert(settled == true, "resolving after the check should still settle the awaiter")
    end

    -- abandoning many deferreds over many rounds must not grow memory, proving the whole cycle is reclaimed
    do
        local function round()
            for _ = 1, 200 do
                local promise = (async.deferred())
                async.spawn(function()
                    promise:await()
                end)
            end
            collectgarbage("collect")
            collectgarbage("collect")
            async.sleep(3):await()
            collectgarbage("collect")
            collectgarbage("collect")
        end

        round()
        local baseline = collectgarbage("count")
        for _ = 1, 10 do
            round()
        end
        local growth = collectgarbage("count") - baseline
        assert(growth < 100, "abandoned deferreds must not leak: memory grew " .. math.floor(growth) .. "KB")
    end

    print("async deferred ok")
end)
