-- broadcasting to a set of connections where one has stopped draining drops only that peer, and the surviving connections still receive the message
local async = require("async")
local http = require("http")
local socket = require("socket")
local crypto = require("crypto")

local host = "127.0.0.1"
local port = 39832

-- each broadcast is large enough that a peer which never reads passes the 16 mb outbound cap within a handful of rounds
local chunkBytes = 1024 * 1024
local rounds = 24
local payload = string.rep("b", chunkBytes)
local stalledPeers = 4

local function clientFrame(text)
    local n = #text
    local mask = crypto.randomBytes(4)
    local out = {}
    for i = 1, n do
        out[i] = string.char((text:byte(i) ~ mask:byte((i - 1) % 4 + 1)) & 0xFF)
    end
    return string.char(0x81, 0x80 + n) .. mask .. table.concat(out)
end

local function parseFrame(buffer)
    if #buffer < 2 then
        return nil, buffer
    end

    local opcode = buffer:byte(1) & 0x0F
    local len = buffer:byte(2) & 0x7F
    local offset = 2
    if len == 126 then
        if #buffer < 4 then
            return nil, buffer
        end
        len = buffer:byte(3) * 256 + buffer:byte(4)
        offset = 4
    elseif len == 127 then
        if #buffer < 10 then
            return nil, buffer
        end
        len = 0
        for i = 3, 10 do
            len = len * 256 + buffer:byte(i)
        end
        offset = 10
    end

    if #buffer < offset + len then
        return nil, buffer
    end

    return { opcode = opcode, payload = buffer:sub(offset + 1, offset + len) }, buffer:sub(offset + len + 1)
end

local app = http.createApp()
app:ws("/fanout", {
    open = function() end,
    message = function()
        app:wsBroadcast("/fanout", payload)
    end,
})
app:listen({ host = host, port = port })

local function openWs()
    local conn = socket.tcp.connect(host, port):await()
    local key = crypto.base64Encode(crypto.randomBytes(16))
    conn:send(table.concat({
        "GET /fanout HTTP/1.1",
        "Host: " .. host,
        "Upgrade: websocket",
        "Connection: Upgrade",
        "Sec-WebSocket-Key: " .. key,
        "Sec-WebSocket-Version: 13",
        "",
        "",
    }, "\r\n")):await()

    local buffer = ""
    while not buffer:find("\r\n\r\n", 1, true) do
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "handshake closed early")
        buffer = buffer .. chunk
    end
    assert(buffer:find("101", 1, true), "handshake did not switch protocols")
    return conn, buffer:sub((buffer:find("\r\n\r\n", 1, true)) + 4)
end

async.run(function()
    -- several peers connect and never read a byte, so each broadcast piles up in their send queues
    local stalled = {}
    for i = 1, stalledPeers do
        stalled[i] = (openWs())
    end

    local reader, buffer = openWs()

    -- the reader drives the broadcasts and drains its own copy, so it must survive every peer the server drops
    local received = 0
    for _ = 1, rounds do
        reader:send(clientFrame("go")):await()

        local frame
        while true do
            frame, buffer = parseFrame(buffer)
            if frame then
                break
            end

            local chunk, err = reader:receive(65536):await()
            assert(chunk and #chunk > 0, "the reading peer was dropped after " .. received .. " rounds: " .. tostring(err))
            buffer = buffer .. chunk
        end

        assert(frame.opcode == 0x1, "the broadcast should be a text frame, got opcode " .. frame.opcode)
        assert(#frame.payload == chunkBytes, "round " .. (received + 1) .. " should arrive whole, got " .. #frame.payload)
        received = received + 1
    end

    assert(received == rounds, "the draining peer should receive every broadcast")

    for _, conn in ipairs(stalled) do
        conn:close():await()
    end
    reader:close():await()

    print("http ws broadcast drop ok")
end)
