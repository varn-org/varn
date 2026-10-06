-- A WebSocket that its open handler already drops, here by queueing more than the route allows, still reports its close and leaves the application.
local async = require("async")
local http = require("http")
local socket = require("socket")
local crypto = require("crypto")

local host = "127.0.0.1"
local closed = nil

local app = http.createApp()
app:ws("/flood", {
    open = function(conn)
        conn:send(string.rep("a", 10))
        conn:send(string.rep("b", 10))
    end,
    close = function(_, code, reason)
        closed = { code = code, reason = reason }
    end,
}, { maxQueuedBytes = 16 })
local server = app:listen({ host = host, port = 0 })

async.run(function()
    local conn = socket.tcp.connect(host, server.port):await()
    conn:send(table.concat({
        "GET /flood HTTP/1.1",
        "Host: " .. host,
        "Upgrade: websocket",
        "Connection: Upgrade",
        "Sec-WebSocket-Key: " .. crypto.base64Encode(crypto.randomBytes(16)),
        "Sec-WebSocket-Version: 13",
        "",
        "",
    }, "\r\n")):await()

    for _ = 1, 100 do
        if closed then
            break
        end
        async.sleep(20):await()
    end

    assert(closed, "The close callback should run for a connection the open handler dropped.")
    assert(closed.code == 1006, "The dropped connection should close with 1006, not " .. tostring(closed.code) .. ".")
    assert(app:wsBroadcast("/flood", "after") == 0, "The dropped connection should leave the application.")

    conn:close():await()
    server:close():await()
    print("The \"http\" WebSocket open tests passed.")
end)
