-- an sse event name or comment occupies a whole line, so a line break in one would forge every field after it, and a payload carrying a bare cr must open a new data field rather than escape the frame
local async = require("async")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"
local port = 39837

local app = http.createApp()

app:get("/named", function(ctx)
    local stream = ctx:sse()
    local ok = pcall(function()
        stream:send("tick\n\nevent: forged\ndata: {\"role\":\"admin\"}", "real")
    end)
    stream:send("result", ok and "accepted" or "rejected")
    stream:close()
end)

app:get("/comment", function(ctx)
    local stream = ctx:sse()
    local ok = pcall(function()
        stream:comment("beat\ndata: forged")
    end)
    stream:send("result", ok and "accepted" or "rejected")
    stream:close()
end)

app:get("/carriage", function(ctx)
    local stream = ctx:sse()
    stream:send("a\rid: 99\rretry: 1")
    stream:close()
end)

app:listen({ host = host, port = port })

local function readStream(path)
    local conn = socket.tcp.connect(host, port):await()
    conn:send("GET " .. path .. " HTTP/1.1\r\nHost: " .. host .. "\r\nAccept: text/event-stream\r\n\r\n"):await()

    local buffer = ""
    while true do
        local chunk = conn:receive(65536):await()
        if not chunk or #chunk == 0 then
            break
        end
        buffer = buffer .. chunk
    end
    conn:close():await()
    return buffer
end

async.run(function()
    -- an event name carrying a break is refused, so no forged event ever reaches the wire
    local named = readStream("/named")
    assert(named:find("rejected", 1, true), "an event name with a line break must be refused")
    assert(not named:find("event: forged", 1, true), "the forged event must never be written")
    assert(not named:find("{\"role\":\"admin\"}", 1, true), "the forged payload must never be written")

    -- a comment carrying a break is refused for the same reason
    local commented = readStream("/comment")
    assert(commented:find("rejected", 1, true), "a comment with a line break must be refused")
    assert(not commented:find("data: forged", 1, true), "the forged data field must never be written")

    -- a bare carriage return inside a payload becomes another data field instead of a new record
    local carriage = readStream("/carriage")
    assert(carriage:find("data: a", 1, true), "the payload before the carriage return stays a data field")
    assert(carriage:find("data: id: 99", 1, true), "text after a carriage return is escaped into its own data field")
    assert(carriage:find("data: retry: 1", 1, true), "every carriage-return segment is escaped")
    assert(not carriage:find("\rid: 99", 1, true), "a raw carriage return must not survive into the stream")

    print("http sse injection ok")
end)
