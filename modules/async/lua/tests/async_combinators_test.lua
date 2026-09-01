-- async combinators covering all order and failure, allSettled capture, race and any selection, timeout firing, mapLimit bounding concurrency while preserving order, empty-list handling, and mapper failures
local async = require("async")

-- a promise that resolves to value after ms milliseconds
local function resolveAfter(ms, value)
    return async.promise(function()
        async.sleep(ms):await()
        return value
    end)
end

-- a promise that rejects with reason after ms milliseconds
local function rejectAfter(ms, reason)
    return async.promise(function()
        async.sleep(ms):await()
        error(reason, 0)
    end)
end

async.run(function()
    -- all collects every result in input order regardless of completion order
    local results = async.all({ resolveAfter(6, "a"), resolveAfter(2, "b"), resolveAfter(4, "c") }):await()
    assert(table.concat(results, ",") == "a,b,c", "all should preserve input order")

    -- all rejects as soon as any input rejects
    local value, err = async.all({ resolveAfter(8, "x"), rejectAfter(2, "boom") }):await()
    assert(value == nil, "all should not resolve when an input rejects")
    assert(err == "boom", "all should surface the first rejection")

    -- allSettled never rejects and reports per-input outcomes in order
    local settled = async.allSettled({ resolveAfter(2, "ok"), rejectAfter(3, "nope") }):await()
    assert(settled[1].ok == true and settled[1].value == "ok", "allSettled should record the resolved value")
    assert(settled[2].ok == false and settled[2].error == "nope", "allSettled should record the rejection")

    -- race settles with the first input to settle, even when that one rejects
    local raced = async.race({ resolveAfter(6, "slow"), resolveAfter(2, "fast") }):await()
    assert(raced == "fast", "race should pick the first to settle")

    local racedValue, racedErr = async.race({ rejectAfter(2, "early"), resolveAfter(6, "late") }):await()
    assert(racedValue == nil and racedErr == "early", "race should surface the first settle even if it rejects")

    -- any resolves with the first input to resolve and ignores earlier rejections
    local won = async.any({ rejectAfter(2, "bad"), resolveAfter(4, "good") }):await()
    assert(won == "good", "any should resolve with the first successful input")

    -- any rejects only when every input rejects
    local anyValue, anyErr = async.any({ rejectAfter(2, "one"), rejectAfter(3, "two") }):await()
    assert(anyValue == nil, "any should reject when all inputs reject")
    assert(type(anyErr) == "string", "any should surface a rejection message")

    -- timeout resolves with the value when the promise settles in time
    local quick = async.timeout(resolveAfter(2, "in time"), 50):await()
    assert(quick == "in time", "timeout should pass through a value that settles in time")

    -- timeout rejects when the promise does not settle within the budget
    local slowValue, slowErr = async.timeout(resolveAfter(50, "too slow"), 3):await()
    assert(slowValue == nil, "timeout should not resolve when the budget elapses")
    assert(type(slowErr) == "string" and slowErr:find("timeout"), "timeout should surface a timeout error")

    -- mapLimit keeps at most limit calls in flight and preserves result order
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
    assert(table.concat(mapped, ",") == "10,20,30,40,50,60", "mapLimit should preserve input order")
    assert(peak <= 2, "mapLimit should never exceed the concurrency limit")
    assert(peak == 2, "mapLimit should reach the concurrency limit")

    -- all and allSettled of an empty list resolve immediately to an empty result
    local emptyAll = async.all({}):await()
    assert(type(emptyAll) == "table" and #emptyAll == 0, "all of an empty list should resolve to an empty table")
    local emptySettled = async.allSettled({}):await()
    assert(type(emptySettled) == "table" and #emptySettled == 0, "allSettled of an empty list should resolve to an empty table")

    -- race of an empty list rejects instead of hanging forever with nothing to settle it
    local emptyRaceValue, emptyRaceErr = async.race({}):await()
    assert(emptyRaceValue == nil, "race of an empty list should not resolve")
    assert(type(emptyRaceErr) == "string" and emptyRaceErr:find("empty"), "race of an empty list should reject")

    -- mapLimit rejects when the mapper raises rather than leaving the map hanging on a lost counter
    local throwValue, throwErr = async.mapLimit({ 1, 2, 3 }, 2, function(item)
        error("mapper boom " .. item)
    end):await()
    assert(throwValue == nil, "mapLimit should not resolve when the mapper raises")
    assert(type(throwErr) == "string" and throwErr:find("boom"), "mapLimit should surface the mapper error")

    -- mapLimit rejects when a mapped promise rejects
    local rejValue, rejErr = async.mapLimit({ 1, 2 }, 1, function(item)
        return rejectAfter(1, "mapped rejection " .. item)
    end):await()
    assert(rejValue == nil, "mapLimit should not resolve when a mapped promise rejects")
    assert(type(rejErr) == "string" and rejErr:find("rejection"), "mapLimit should surface a mapped rejection")

    -- mapLimit rejects an invalid concurrency limit instead of spinning forever on a gate that can never open
    local zeroOk, zeroErr = pcall(function()
        return async.mapLimit({ 1, 2, 3 }, 0, function(item)
            return async.promise(function() return item end)
        end)
    end)
    assert(zeroOk == false, "mapLimit with limit 0 should error")
    assert(type(zeroErr) == "string" and zeroErr:find("at least 1"), "mapLimit should reject a non-positive limit")
    assert(pcall(function() return async.mapLimit({ 1 }, -1, function(x) return async.promise(function() return x end) end) end) == false, "mapLimit with a negative limit should error")

    -- empty-list mapLimit resolves to an empty table and empty-list any rejects
    local emptyMapped = async.mapLimit({}, 3, function(x) return async.promise(function() return x end) end):await()
    assert(type(emptyMapped) == "table" and #emptyMapped == 0, "mapLimit of an empty list should resolve to an empty table")
    local emptyAnyValue, emptyAnyErr = async.any({}):await()
    assert(emptyAnyValue == nil, "any of an empty list should not resolve")
    assert(type(emptyAnyErr) == "string", "any of an empty list should reject")

    -- a large list through mapLimit preserves order and never exceeds the limit under load
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
    assert(#stressOut == 200, "mapLimit stress should map every item")
    for i = 1, 200 do
        assert(stressOut[i] == i, "mapLimit stress should preserve order at index " .. i)
    end
    assert(stressPeak <= 8, "mapLimit stress should never exceed the limit")
    assert(stressPeak == 8, "mapLimit stress should reach the limit under load")

    print("async combinators ok")
end)
