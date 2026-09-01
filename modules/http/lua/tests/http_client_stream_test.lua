-- the client consumes a streaming sse response chunk by chunk through client.stream
local async = require("async")
local http = require("http")

local port = 39833
local base = "http://127.0.0.1:" .. port

local app = http.createApp()

-- streams a few sse events then closes so the client sees a finite body delivered in pieces
app:get("/events", function(ctx)
    local stream = ctx:sse()
    stream:send("hello")
    stream:send("tick", "first")
    stream:send("done")
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

    assert(done == "ok", "stream did not complete")
    assert(seenStatus == 200, "stream status not 200")
    assert(seenType and seenType:find("text/event-stream", 1, true), "unexpected content-type: " .. tostring(seenType))
    assert(chunkCount > 0, "onChunk was never called")

    local body = table.concat(collected)
    assert(body:find("hello", 1, true), "missing first sse event")
    assert(body:find("tick", 1, true), "missing named sse event")
    assert(body:find("done", 1, true), "missing final sse event")

    print("http client stream ok")
end)
