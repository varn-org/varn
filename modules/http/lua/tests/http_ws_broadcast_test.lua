-- websocket broadcast and rooms where two clients upgrade, join a room, and a message from one fans out to all members
local async = require("async")
local http = require("http")
local socket = require("socket")
local crypto = require("crypto")

local host = "127.0.0.1"
local port = 39827

-- builds a masked client text frame, the only framing the server accepts from a client
local function clientFrame(payload)
    local mask = crypto.randomBytes(4)
    local header = string.char(0x81)
    local n = #payload
    if n < 126 then
        header = header .. string.char(0x80 + n)
    else
        header = header .. string.char(0x80 + 126, math.floor(n / 256) % 256, n % 256)
    end
    local masked = {}
    for i = 1, n do
        local m = mask:byte((i - 1) % 4 + 1)
        masked[i] = string.char((payload:byte(i) ~ m) & 0xFF)
    end
    return header .. mask .. table.concat(masked)
end

-- parses one unmasked server text frame out of a buffer, returning the payload and the leftover bytes
local function parseServerFrame(buffer)
    if #buffer < 2 then
        return nil, buffer
    end
    local len = buffer:byte(2) & 0x7F
    local offset = 2
    if len == 126 then
        if #buffer < 4 then
            return nil, buffer
        end
        len = buffer:byte(3) * 256 + buffer:byte(4)
        offset = 4
    end
    if #buffer < offset + len then
        return nil, buffer
    end
    return buffer:sub(offset + 1, offset + len), buffer:sub(offset + len + 1)
end

local app = http.createApp()

-- every connection joins a shared room on open and rebroadcasts any message to that room
app:ws("/room", {
    open = function(conn)
        conn:join("lobby")
    end,
    message = function(conn, data)
        app:wsBroadcastRoom("lobby", "room:" .. data)
    end,
})

-- a plain endpoint exercises the path-scoped broadcast
app:ws("/feed", {
    open = function() end,
    message = function(_, data)
        app:wsBroadcast("/feed", "feed:" .. data)
    end,
})

app:listen({ host = host, port = port })

-- opens a websocket and returns the connected socket past the handshake
local function openWs(path)
    local conn = socket.tcp.connect(host, port):await()
    local key = crypto.base64Encode(crypto.randomBytes(16))
    local handshake = table.concat({
        "GET " .. path .. " HTTP/1.1",
        "Host: " .. host,
        "Upgrade: websocket",
        "Connection: Upgrade",
        "Sec-WebSocket-Key: " .. key,
        "Sec-WebSocket-Version: 13",
        "",
        "",
    }, "\r\n")
    conn:send(handshake):await()

    -- read until the handshake response terminator, leaving any frame bytes untouched
    local buffer = ""
    while not buffer:find("\r\n\r\n", 1, true) do
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "handshake closed early")
        buffer = buffer .. chunk
    end
    assert(buffer:find("101", 1, true), "handshake did not switch protocols")
    return conn
end

-- waits for one server text frame on a socket
local function recvFrame(conn)
    local buffer = ""
    while true do
        local payload, rest = parseServerFrame(buffer)
        if payload then
            return payload
        end
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "connection closed before a frame arrived")
        buffer = (rest or buffer) .. chunk
    end
end

async.run(function()
    -- two members of the same room both receive a broadcast triggered by one of them
    local a = openWs("/room")
    local b = openWs("/room")
    async.sleep(50):await()

    a:send(clientFrame("hi")):await()
    assert(recvFrame(a) == "room:hi", "broadcaster did not receive room message")
    assert(recvFrame(b) == "room:hi", "peer did not receive room message")

    -- a path-scoped broadcast reaches every connection on that ws path
    local c = openWs("/feed")
    local d = openWs("/feed")
    async.sleep(50):await()

    c:send(clientFrame("ping")):await()
    assert(recvFrame(c) == "feed:ping", "feed broadcaster did not receive message")
    assert(recvFrame(d) == "feed:ping", "feed peer did not receive message")

    a:close():await()
    b:close():await()
    c:close():await()
    d:close():await()

    print("http ws broadcast ok")
end)
