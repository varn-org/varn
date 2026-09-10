-- The client consumes a streaming SSE response chunk by chunk through `client.stream`.
local async = require("async")
local http = require("http")

local port = 39833
local base = "http://127.0.0.1:" .. port

local app = http.createApp()

-- Streams a few SSE events then closes so the client sees a finite body delivered in pieces.
app:get("/events", function(ctx)
    local stream = ctx:sse()
    stream:send("hello")
    stream:send("tick", "first")
    stream:send("done")
    stream:close()
end)

-- Sends one event, waits a second and sends another, so a client that waits for more before it hands a piece over is caught.
local secondSent = false
app:get("/paced", function(ctx)
    local stream = ctx:sse()
    stream:send("first")
    async.sleep(1000):await()
    secondSent = true
    stream:send("second")
    stream:close()
end)

app:listen({ host = "127.0.0.1", port = port })

async.run(function()
    local seenStatus
    local seenType
    local chunkCount = 0
    local collected = {}

    local done = http.client.stream({
        url = base .. "/events",
        method = "GET",
        timeoutSeconds = 10,
        onResponse = function(status, headers)
            seenStatus = status
            seenType = headers["content-type"]
        end,
    }, function(chunk)
        chunkCount = chunkCount + 1
        collected[#collected + 1] = chunk
    end):await()

    assert(done == "ok", "The stream did not complete")
    assert(seenStatus == 200, "The stream status is not 200")
    assert(seenType and seenType:find("text/event-stream", 1, true), "Unexpected \"content-type\": " .. tostring(seenType))
    assert(chunkCount > 0, "The callback \"onChunk\" was never called")

    local body = table.concat(collected)
    assert(body:find("hello", 1, true), "Missing first SSE event")
    assert(body:find("tick", 1, true), "Missing named SSE event")
    assert(body:find("done", 1, true), "Missing final SSE event")

    -- Each piece reaches the callback as it arrives, not once the body ends or a buffer fills.
    local firstBeforeSecond = false
    http.client.stream({ url = base .. "/paced", timeoutSeconds = 10 }, function(chunk)
        if chunk:find("first", 1, true) then
            firstBeforeSecond = not secondSent
        end
    end):await()
    assert(firstBeforeSecond, "The first event must arrive before the second one is sent.")

    print("The \"http\" client stream tests passed.")
end)
