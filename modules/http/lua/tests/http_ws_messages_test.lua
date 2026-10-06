-- WebSocket messages keep whether they are binary both ways, a text frame that is not UTF-8 closes with code 1007, close codes and reasons travel both ways, and a route that names its protocols negotiates one of them.
local async = require("async")
local crypto = require("crypto")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"

-- Builds a masked client frame.
local function clientFrame(opcode, payload, fin)
    local first = ((fin == false) and 0 or 0x80) | opcode
    local n = #payload
    local head
    if n < 126 then
        head = string.char(first, 0x80 | n)
    elseif n < 65536 then
        head = string.char(first, 0x80 | 126, n >> 8, n & 0xFF)
    else
        head = string.char(first, 0x80 | 127) .. string.pack(">I8", n)
    end
    local key = crypto.randomBytes(4)
    local out = {}
    for i = 1, n do
        out[i] = string.char(payload:byte(i) ~ key:byte((i - 1) % 4 + 1))
    end
    return head .. key .. table.concat(out)
end

-- Reads one unmasked server frame, keeping what follows it in the buffer of the reader, and answers nil once the server closed the connection.
local function readFrame(reader)
    while true do
        local buffer = reader.buffer
        if #buffer >= 2 then
            local len = buffer:byte(2) & 0x7F
            local offset = 2
            if len == 126 and #buffer >= 4 then
                len = (buffer:byte(3) << 8) | buffer:byte(4)
                offset = 4
            elseif len == 127 and #buffer >= 10 then
                len = string.unpack(">I8", buffer, 3)
                offset = 10
            end
            if (len < 126 or offset > 2) and #buffer >= offset + len then
                reader.buffer = buffer:sub(offset + len + 1)
                return { opcode = buffer:byte(1) & 0x0F, payload = buffer:sub(offset + 1, offset + len) }
            end
        end
        local chunk = reader.conn:receive(65536):await()
        if not chunk or #chunk == 0 then
            return nil
        end
        reader.buffer = reader.buffer .. chunk
    end
end

-- Opens a WebSocket and answers the reader and the head of the answer, which is not a switch when the server refused.
local function openWs(port, path, protocols)
    local conn = socket.tcp.connect(host, port):await()
    local lines = {
        "GET " .. path .. " HTTP/1.1",
        "Host: " .. host,
        "Upgrade: websocket",
        "Connection: Upgrade",
        "Sec-WebSocket-Key: " .. crypto.base64Encode(crypto.randomBytes(16)),
        "Sec-WebSocket-Version: 13",
    }
    if protocols then
        lines[#lines + 1] = "Sec-WebSocket-Protocol: " .. protocols
    end
    lines[#lines + 1] = ""
    lines[#lines + 1] = ""
    conn:send(table.concat(lines, "\r\n")):await()

    local buffer = ""
    while not buffer:find("\r\n\r\n", 1, true) do
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "The handshake closed early")
        buffer = buffer .. chunk
    end
    local headEnd = buffer:find("\r\n\r\n", 1, true)
    return { conn = conn, buffer = buffer:sub(headEnd + 4) }, buffer:sub(1, headEnd)
end

local function closeCode(frame)
    return (frame.payload:byte(1) << 8) | frame.payload:byte(2)
end

async.run(function()
    local closes = {}
    local closed, closeSeen = async.deferred()
    local app = http.createApp()
    app:ws("/echo", {
        message = function(conn, data, binary)
            if data == "close-me" then
                conn:close(4000, "done")
                return
            end
            if data == "bad-code" then
                local ok, err = pcall(conn.close, conn, 1005)
                conn:send(ok and "accepted" or tostring(err))
                return
            end
            conn:send(data, { binary = binary })
        end,
        close = function(_, code, reason)
            closes[#closes + 1] = { code = code, reason = reason }
            closeSeen()
        end,
    })
    app:ws("/proto", {
        open = function(conn) conn:send("protocol " .. tostring(conn.protocol)) end,
    }, { protocols = { "chat.v2", "chat.v1" } })
    app:ws("/news", {})
    local server = app:listen({ host = host, port = 0 })

    -- A binary message reaches the handler as binary and goes back as a binary frame.
    local reader = openWs(server.port, "/echo")
    local bytes = string.char(0, 1, 2, 255, 254)
    reader.conn:send(clientFrame(0x2, bytes)):await()
    local echoed = readFrame(reader)
    assert(echoed.opcode == 0x2 and echoed.payload == bytes, "A binary message is echoed as a binary frame")

    -- A text message stays text, and a large one arrives whole.
    reader.conn:send(clientFrame(0x1, "h\xC3\xA9llo")):await()
    echoed = readFrame(reader)
    assert(echoed.opcode == 0x1 and echoed.payload == "h\xC3\xA9llo", "A text message is echoed as a text frame")
    local large = string.rep("z", 70000)
    reader.conn:send(clientFrame(0x2, large)):await()
    echoed = readFrame(reader)
    assert(echoed.opcode == 0x2 and echoed.payload == large, "A large binary message is echoed whole")

    -- A close code the protocol keeps for itself is refused.
    reader.conn:send(clientFrame(0x1, "bad-code")):await()
    echoed = readFrame(reader)
    assert(echoed.payload:find("cannot be sent", 1, true), "A reserved close code is refused, got: " .. echoed.payload)

    -- The server closes with its code and reason, and its close callback hears them.
    reader.conn:send(clientFrame(0x1, "close-me")):await()
    local goodbye = readFrame(reader)
    assert(goodbye.opcode == 0x8 and closeCode(goodbye) == 4000 and goodbye.payload:sub(3) == "done", "The server closes with code 4000 and its reason")
    closed:await()
    assert(closes[1].code == 4000 and closes[1].reason == "done", "The close callback hears the code the server sent")
    reader.conn:close():await()

    -- A text frame that is not UTF-8 closes the connection with code 1007.
    closed, closeSeen = async.deferred()
    reader = openWs(server.port, "/echo")
    reader.conn:send(clientFrame(0x1, "\xFF\xFE")):await()
    goodbye = readFrame(reader)
    assert(goodbye.opcode == 0x8 and closeCode(goodbye) == 1007, "Text that is not UTF-8 closes with code 1007")
    closed:await()
    assert(closes[2].code == 1007, "The close callback hears code 1007")
    reader.conn:close():await()

    -- The close of the peer is answered with its code, and the callback hears the code and the reason.
    closed, closeSeen = async.deferred()
    reader = openWs(server.port, "/echo")
    reader.conn:send(clientFrame(0x8, string.char(0x0F, 0xA1) .. "bye")):await()
    goodbye = readFrame(reader)
    assert(goodbye.opcode == 0x8 and closeCode(goodbye) == 4001, "The close of the peer is answered with its code")
    closed:await()
    assert(closes[3].code == 4001 and closes[3].reason == "bye", "The close callback hears the code and the reason of the peer")
    reader.conn:close():await()

    -- A close without a code reaches the callback as 1005.
    closed, closeSeen = async.deferred()
    reader = openWs(server.port, "/echo")
    reader.conn:send(clientFrame(0x8, "")):await()
    goodbye = readFrame(reader)
    assert(goodbye.opcode == 0x8 and goodbye.payload == "", "A close without a code is answered without one")
    closed:await()
    assert(closes[4].code == 1005, "A close without a code reaches the callback as 1005")
    reader.conn:close():await()

    -- A peer that leaves without a close reaches the callback as 1006.
    closed, closeSeen = async.deferred()
    reader = openWs(server.port, "/echo")
    reader.conn:close():await()
    closed:await()
    assert(closes[5].code == 1006, "A connection lost without a close reaches the callback as 1006")

    -- The route picks the first of its protocols the client offers and names it in the answer.
    local proto, head = openWs(server.port, "/proto", "chat.v1, chat.v2")
    assert(head:find("Sec-WebSocket-Protocol: chat.v2", 1, true), "The answer names the protocol the route prefers")
    local greeting = readFrame(proto)
    assert(greeting.payload == "protocol chat.v2", "The connection tells its protocol, got: " .. greeting.payload)
    proto.conn:close():await()

    local _, refusedHead = openWs(server.port, "/proto", "other")
    assert(refusedHead:find("^HTTP/1.1 400"), "A client offering none of the protocols of the route is refused")
    local _, missingHead = openWs(server.port, "/proto")
    assert(missingHead:find("^HTTP/1.1 400"), "A client offering no protocol to a route that names some is refused")

    -- A route that names no protocol answers without one.
    local news, newsHead = openWs(server.port, "/news", "chat.v1")
    assert(newsHead:find(" 101 ", 1, true) and not newsHead:find("Sec-WebSocket-Protocol", 1, true), "A route without protocols names none")

    -- A broadcast can send binary frames.
    async.sleep(20):await()
    assert(app:wsBroadcast("/news", bytes, { binary = true }) == 1, "The broadcast reaches the one connection")
    local broadcast = readFrame(news)
    assert(broadcast.opcode == 0x2 and broadcast.payload == bytes, "A binary broadcast arrives as a binary frame")
    assert(app:wsBroadcast("/news", "text") == 1, "A text broadcast reaches the connection")
    broadcast = readFrame(news)
    assert(broadcast.opcode == 0x1 and broadcast.payload == "text", "A broadcast is text by default")
    news.conn:close():await()

    -- Unknown options are refused by name.
    local ok, err = pcall(app.ws, app, "/bad", {}, { maxMessages = 1 })
    assert(not ok and tostring(err):find('The WebSocket option "maxMessages" is unknown.', 1, true), "An unknown option of a route is refused, got: " .. tostring(err))
    ok, err = pcall(app.wsBroadcast, app, "/news", "x", { binery = true })
    assert(not ok and tostring(err):find('The send option "binery" is unknown.', 1, true), "An unknown send option is refused, got: " .. tostring(err))

    print("The \"http\" WebSocket messages tests passed.")
end)
