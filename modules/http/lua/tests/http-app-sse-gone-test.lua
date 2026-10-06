-- An event stream learns that its client left, since each send answers whether its record left for the client, so a writer that loops on its sends ends.
local async = require("async")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"
local sent = 0
local finished = false

local app = http.createApp()
app:get("/events", function(ctx)
    local stream = ctx:sse()
    while stream:send("tick"):await() do
        sent = sent + 1
        async.sleep(20):await()
    end
    finished = true
end)
local server = app:listen({ host = host, port = 0 })

async.run(function()
    local conn = socket.tcp.connect(host, server.port):await()
    conn:send("GET /events HTTP/1.1\r\nHost: " .. host .. "\r\n\r\n"):await()

    local buffer = ""
    while not buffer:find("data: tick", 1, true) do
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "The stream closed before its first record.")
        buffer = buffer .. chunk
    end
    conn:close():await()

    for _ = 1, 150 do
        if finished then
            break
        end
        async.sleep(20):await()
    end

    assert(finished, "A send should answer false once the client left, which ends the loop of the writer.")
    assert(sent > 0, "The records before the client left should have been sent.")

    server:close():await()
    print("The \"http\" event stream tests passed.")
end)
