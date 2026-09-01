-- websocket control frames where ping auto-pongs, a fragmented message reassembles, close is echoed, and an unmasked or reserved-bit frame drops the connection
local async = require("async")
local http = require("http")
local socket = require("socket")
local crypto = require("crypto")

local host = "127.0.0.1"
local port = 39828

-- builds a client frame with an explicit opcode, fin flag and masking
local function frame(opcode, payload, fin, masked)
    fin = fin ~= false
    masked = masked ~= false
    local b0 = (fin and 0x80 or 0) | opcode
    local maskbit = masked and 0x80 or 0
    local n = #payload
    local header = string.char(b0, maskbit + n)
    if not masked then
        return header .. payload
    end
    local mask = crypto.randomBytes(4)
    local out = {}
    for i = 1, n do
        out[i] = string.char((payload:byte(i) ~ mask:byte((i - 1) % 4 + 1)) & 0xFF)
    end
    return header .. mask .. table.concat(out)
end

-- parses one unmasked server frame, returning its opcode and payload plus the leftover bytes
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
    end
    if #buffer < offset + len then
        return nil, buffer
    end
    return { opcode = opcode, payload = buffer:sub(offset + 1, offset + len) }, buffer:sub(offset + len + 1)
end

local app = http.createApp()
app:ws("/echo", {
    open = function() end,
    message = function(_, data)
        app:wsBroadcast("/echo", data)
    end,
})
app:listen({ host = host, port = port })

local function openWs()
    local conn = socket.tcp.connect(host, port):await()
    local key = crypto.base64Encode(crypto.randomBytes(16))
    conn:send(table.concat({
        "GET /echo HTTP/1.1",
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
    return conn
end

local function recvFrame(conn)
    local buffer = ""
    while true do
        local decoded, rest = parseFrame(buffer)
        if decoded then
            return decoded
        end
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "connection closed before a frame arrived")
        buffer = (rest or buffer) .. chunk
    end
end

async.run(function()
    -- a ping is answered with a pong carrying the same payload
    local c = openWs()
    c:send(frame(0x9, "hi")):await()
    local pong = recvFrame(c)
    assert(pong.opcode == 0xA and pong.payload == "hi", "a ping should be answered with a matching pong")

    -- a fragmented text message reassembles before reaching the handler
    c:send(frame(0x1, "he", false, true)):await()
    c:send(frame(0x0, "llo", true, true)):await()
    local echoed = recvFrame(c)
    assert(echoed.opcode == 0x1 and echoed.payload == "hello", "fragments should reassemble into one message")

    -- a close frame is echoed to complete the close handshake
    c:send(frame(0x8, string.char(0x03, 0xE8))):await()
    local closed = recvFrame(c)
    assert(closed.opcode == 0x8, "the server should echo a close frame")
    c:close():await()

    -- an unmasked client frame is a protocol error and drops the connection
    local u = openWs()
    u:send(frame(0x1, "nope", true, false)):await()
    local uData, uErr = u:receive(4096):await()
    assert(uData == "" or uErr, "an unmasked frame should drop the connection")
    u:close():await()

    -- a reserved-bit frame is a protocol error and drops the connection
    local r = openWs()
    local rsv = string.char(0xC1, 0x80 + 2) .. crypto.randomBytes(4) .. "xx"
    r:send(rsv):await()
    local rData, rErr = r:receive(4096):await()
    assert(rData == "" or rErr, "a reserved-bit frame should drop the connection")
    r:close():await()

    print("http ws protocol ok")
end)
