-- a websocket whose peer keeps draining stays open past the outbound cap in cumulative traffic, since the cap bounds the outstanding backlog and the sent prefix is reclaimed rather than counted forever
local async = require("async")
local http = require("http")
local socket = require("socket")
local crypto = require("crypto")

local host = "127.0.0.1"
local port = 39831

-- two bursts of this size push the cumulative total past the 16 mb cap while no single backlog ever approaches it
local frameBytes = 512 * 1024
local burst = 18
local payload = string.rep("v", frameBytes)

local function clientFrame(text)
    local n = #text
    local mask = crypto.randomBytes(4)
    local out = {}
    for i = 1, n do
        out[i] = string.char((text:byte(i) ~ mask:byte((i - 1) % 4 + 1)) & 0xFF)
    end
    return string.char(0x81, 0x80 + n) .. mask .. table.concat(out)
end

-- reads one unmasked server frame off the buffer, returning it plus the unconsumed remainder
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
app:ws("/bulk", {
    open = function() end,
    message = function(conn)
        conn:send(payload)
    end,
})
app:listen({ host = host, port = port })

async.run(function()
    local conn = socket.tcp.connect(host, port):await()
    local key = crypto.base64Encode(crypto.randomBytes(16))
    conn:send(table.concat({
        "GET /bulk HTTP/1.1",
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
    buffer = buffer:sub((buffer:find("\r\n\r\n", 1, true)) + 4)

    local read = 0
    local function readFrames(count)
        for _ = 1, count do
            local frame
            while true do
                frame, buffer = parseFrame(buffer)
                if frame then
                    break
                end

                local chunk, err = conn:receive(65536):await()
                assert(chunk and #chunk > 0, "the connection was dropped after " .. read .. " frames: " .. tostring(err))
                buffer = buffer .. chunk
            end

            assert(frame.opcode == 0x1, "the reply should be a text frame, got opcode " .. frame.opcode)
            assert(frame.payload == payload, "frame " .. (read + 1) .. " should arrive intact and whole")
            read = read + 1
        end
    end

    local function sendBurst()
        for _ = 1, burst do
            conn:send(clientFrame("more")):await()
        end
    end

    -- the first burst queues nine megabytes, and draining only half of it leaves several megabytes outstanding
    sendBurst()
    readFrames(burst // 2)

    -- the second burst arrives while that tail is still unsent, so the queue holds a large already-sent prefix on top of a small live backlog
    sendBurst()
    readFrames(burst * 2 - (burst // 2))

    assert(read == burst * 2, "every frame should have been delivered")
    conn:close():await()

    print(string.format("http ws backlog ok (%d mb across %d frames)", (read * frameBytes) // (1024 * 1024), read))
end)
