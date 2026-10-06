-- A promise settled with a long string hands every awaiter the same Lua string, whether it waited before the promise settled or awaited it afterwards, so many awaiters never copy a large value.
local async = require("async")
local fs = require("fs")

local dir = os.getenv("VARN_TEST_DIR") or "."
local path = dir .. "/shared-value.bin"
local size = 8 * 1024 * 1024
local awaiters = 10

local function heapKb()
    collectgarbage("collect")
    collectgarbage("collect")
    return collectgarbage("count")
end

async.run(function()
    fs.writeFile(path, string.rep("s", size)):await()
    collectgarbage("collect")
    local before = heapKb()

    -- The awaiters wait before the native read settles the promise.
    local promise = fs.readFile(path)
    local kept = {}
    local finished = 0
    for index = 1, awaiters do
        async.spawn(function()
            kept[index] = promise:await()
            finished = finished + 1
        end)
    end

    while finished < awaiters do
        async.sleep(1):await()
    end

    for index = 1, awaiters do
        assert(#kept[index] == size, "Every awaiter should receive the whole value")
    end

    local grown = heapKb() - before
    assert(grown < 2 * size / 1024, "The awaiters should share one string, but the heap grew by " .. math.floor(grown) .. " KiB")

    -- Awaiting the settled promise again hands out the same string.
    for index = 1, awaiters do
        kept[awaiters + index] = promise:await()
    end

    grown = heapKb() - before
    assert(grown < 2 * size / 1024, "Later awaiters should share the string too, but the heap grew by " .. math.floor(grown) .. " KiB")

    -- A short value is interned by Lua and reaches every awaiter the same way.
    local short = fs.writeFile(path, "x")
    for _ = 1, awaiters do
        assert(short:await() == "ok", "A short value should reach every awaiter")
    end

    print("The \"async\" shared value tests passed.")
end)
