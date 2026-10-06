-- Async combinators covering `all` order and failure, `allSettled` capture, `race` and `any` selection, `timeout` firing, `mapLimit` bounding concurrency while preserving order, empty-list handling, and mapper failures.
local async = require("async")

-- A promise that resolves to `value` after `ms` milliseconds.
local function resolveAfter(ms, value)
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

-- A promise that rejects with `reason` after `ms` milliseconds.
local function rejectAfter(ms, reason)
    return async.promise(function()
        async.sleep(ms):await()
        error(reason, 0)
    end)
end

async.run(function()
    -- The `all` combinator collects every result in input order regardless of completion order.
    local results = async.all({ resolveAfter(6, "a"), resolveAfter(2, "b"), resolveAfter(4, "c") }):await()
    assert(table.concat(results, ",") == "a,b,c", 'The "all" combinator should preserve input order.')

    -- The `all` combinator rejects as soon as any input rejects.
    local value, err = async.all({ resolveAfter(8, "x"), rejectAfter(2, "Boom") }):await()
    assert(value == nil, 'The "all" combinator should not resolve when an input rejects.')
    assert(err == "Boom", 'The "all" combinator should surface the first rejection.')

    -- The `allSettled` combinator never rejects and reports per-input outcomes in order.
    local settled = async.allSettled({ resolveAfter(2, "ok"), rejectAfter(3, "Nope") }):await()
    assert(settled[1].ok == true and settled[1].value == "ok", 'The "allSettled" combinator should record the resolved value.')
    assert(settled[2].ok == false and settled[2].error == "Nope", 'The "allSettled" combinator should record the rejection.')

    -- The `race` combinator settles with the first input to settle, even when that one rejects.
    local raced = async.race({ resolveAfter(6, "slow"), resolveAfter(2, "fast") }):await()
    assert(raced == "fast", 'The "race" combinator should pick the first to settle.')

    local racedValue, racedErr = async.race({ rejectAfter(2, "Early"), resolveAfter(6, "late") }):await()
    assert(racedValue == nil and racedErr == "Early", 'The "race" combinator should surface the first settle even if it rejects.')

    -- The `any` combinator resolves with the first input to resolve and ignores earlier rejections.
    local won = async.any({ rejectAfter(2, "Bad"), resolveAfter(4, "good") }):await()
    assert(won == "good", 'The "any" combinator should resolve with the first successful input.')

    -- The `any` combinator rejects only when every input rejects.
    local anyValue, anyErr = async.any({ rejectAfter(2, "One"), rejectAfter(3, "Two") }):await()
    assert(anyValue == nil, 'The "any" combinator should reject when all inputs reject.')
    assert(type(anyErr) == "string", 'The "any" combinator should surface a rejection message.')

    -- The `timeout` combinator resolves with the value when the promise settles in time.
    local quick = async.timeout(resolveAfter(2, "in time"), 50):await()
    assert(quick == "in time", 'The "timeout" combinator should pass through a value that settles in time.')

    -- The `timeout` combinator rejects when the promise does not settle within the budget.
    local slowValue, slowErr = async.timeout(resolveAfter(50, "too slow"), 3):await()
    assert(slowValue == nil, 'The "timeout" combinator should not resolve when the budget elapses.')
    assert(type(slowErr) == "string" and slowErr:find("timeout"), 'The "timeout" combinator should surface a timeout error.')

    -- The `mapLimit` combinator keeps at most `limit` calls in flight and preserves result order.
    local items = { 1, 2, 3, 4, 5, 6 }
    local inFlight = 0
    local peak = 0
    local mapped = async.mapLimit(items, 2, function(item)
        return async.promise(function()
            inFlight = inFlight + 1
            if inFlight > peak then
                peak = inFlight
            end
            async.sleep(3):await()
            inFlight = inFlight - 1
            return item * 10
        end)
    end):await()
    assert(table.concat(mapped, ",") == "10,20,30,40,50,60", 'The "mapLimit" combinator should preserve input order.')
    assert(peak <= 2, 'The "mapLimit" combinator should never exceed the concurrency limit.')
    assert(peak == 2, 'The "mapLimit" combinator should reach the concurrency limit.')

    -- The `all` and `allSettled` combinators of an empty list resolve immediately to an empty result.
    local emptyAll = async.all({}):await()
    assert(type(emptyAll) == "table" and #emptyAll == 0, 'The "all" combinator of an empty list should resolve to an empty table.')
    local emptySettled = async.allSettled({}):await()
    assert(type(emptySettled) == "table" and #emptySettled == 0, 'The "allSettled" combinator of an empty list should resolve to an empty table.')

    -- The `race` combinator of an empty list rejects instead of hanging forever with nothing to settle it.
    local emptyRaceValue, emptyRaceErr = async.race({}):await()
    assert(emptyRaceValue == nil, 'The "race" combinator of an empty list should not resolve.')
    assert(type(emptyRaceErr) == "string" and emptyRaceErr:find("empty"), 'The "race" combinator of an empty list should reject.')

    -- The `mapLimit` combinator rejects when the mapper raises rather than leaving the map hanging on a lost counter.
    local throwValue, throwErr = async.mapLimit({ 1, 2, 3 }, 2, function(item)
        error("Mapper boom " .. item)
    end):await()
    assert(throwValue == nil, 'The "mapLimit" combinator should not resolve when the mapper raises.')
    assert(type(throwErr) == "string" and throwErr:find("boom"), 'The "mapLimit" combinator should surface the mapper error.')

    -- The `mapLimit` combinator rejects when a mapped promise rejects.
    local rejValue, rejErr = async.mapLimit({ 1, 2 }, 1, function(item)
        return rejectAfter(1, "Mapped rejection " .. item)
    end):await()
    assert(rejValue == nil, 'The "mapLimit" combinator should not resolve when a mapped promise rejects.')
    assert(type(rejErr) == "string" and rejErr:find("rejection"), 'The "mapLimit" combinator should surface a mapped rejection.')

    -- The `mapLimit` combinator rejects an invalid concurrency limit instead of spinning forever on a gate that can never open.
    local zeroOk, zeroErr = pcall(function()
        return async.mapLimit({ 1, 2, 3 }, 0, function(item)
            return async.promise(function() return item end)
        end)
    end)
    assert(zeroOk == false, 'The "mapLimit" combinator with a limit of 0 should error.')
    assert(type(zeroErr) == "string" and zeroErr:find("at least 1"), 'The "mapLimit" combinator should reject a non-positive limit.')
    assert(pcall(function() return async.mapLimit({ 1 }, -1, function(x) return async.promise(function() return x end) end) end) == false, 'The "mapLimit" combinator with a negative limit should error.')

    -- An empty-list `mapLimit` resolves to an empty table and an empty-list `any` rejects.
    local emptyMapped = async.mapLimit({}, 3, function(x) return async.promise(function() return x end) end):await()
    assert(type(emptyMapped) == "table" and #emptyMapped == 0, 'The "mapLimit" combinator of an empty list should resolve to an empty table.')
    local emptyAnyValue, emptyAnyErr = async.any({}):await()
    assert(emptyAnyValue == nil, 'The "any" combinator of an empty list should not resolve.')
    assert(type(emptyAnyErr) == "string", 'The "any" combinator of an empty list should reject.')

    -- A large list through `mapLimit` preserves order and never exceeds the limit under load.
    local big = {}
    for i = 1, 200 do
        big[i] = i
    end
    local stressInFlight = 0
    local stressPeak = 0
    local stressOut = async.mapLimit(big, 8, function(item)
        return async.promise(function()
            stressInFlight = stressInFlight + 1
            if stressInFlight > stressPeak then
                stressPeak = stressInFlight
            end
            async.sleep(1):await()
            stressInFlight = stressInFlight - 1
            return item
        end)
    end):await()
    assert(#stressOut == 200, 'The "mapLimit" stress run should map every item.')
    for i = 1, 200 do
        assert(stressOut[i] == i, 'The "mapLimit" stress run should preserve order at index ' .. i)
    end
    assert(stressPeak <= 8, 'The "mapLimit" stress run should never exceed the limit.')
    assert(stressPeak == 8, 'The "mapLimit" stress run should reach the limit under load.')

    print('The "async" combinator tests passed.')
end)
