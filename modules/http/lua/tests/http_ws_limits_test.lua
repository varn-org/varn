-- A WebSocket server pings and hears pongs, drops a peer that stops answering its pings, closes with code 1009 past the message limit, and keeps its send queue within `maxQueuedBytes`, with sends that resolve once their message left and a `bufferedAmount` that tells what waits.
local async = require("async")
local crypto = require("crypto")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"

-- Builds a masked client frame.
local function clientFrame(opcode, payload)
    local n = #payload
    local head
    if n < 126 then
        head = string.char(0x80 | opcode, 0x80 | n)
    else
        head = string.char(0x80 | opcode, 0x80 | 126, n >> 8, n & 0xFF)
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

local function openWs(port, path)
    local conn = socket.tcp.connect(host, port):await()
    conn:send(table.concat({
        "GET " .. path .. " HTTP/1.1",
        "Host: " .. host,
        "Upgrade: websocket",
        "Connection: Upgrade",
        "Sec-WebSocket-Key: " .. crypto.base64Encode(crypto.randomBytes(16)),
        "Sec-WebSocket-Version: 13",
        "",
        "",
    }, "\r\n")):await()

    local buffer = ""
    while not buffer:find("\r\n\r\n", 1, true) do
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "The handshake closed early")
        buffer = buffer .. chunk
    end
    assert(buffer:find(" 101 ", 1, true), "The handshake did not switch protocols")
    return { conn = conn, buffer = buffer:sub((buffer:find("\r\n\r\n", 1, true)) + 4) }
end

local function closeCode(frame)
    return (frame.payload:byte(1) << 8) | frame.payload:byte(2)
end

async.run(function()
    -- Each route keeps the first close it hears and settles the promise of that route.
    local closes = {}
    local closedRoutes = {}
    local function recorder(route)
        local promise, settle = async.deferred()
        closedRoutes[route] = promise
        return function(_, code, reason)
            closes[route] = closes[route] or { code = code, reason = reason }
            settle()
        end
    end

    local queued = {}
    local app = http.createApp()

    -- The server pings on open and answers each pong with its payload.
    app:ws("/ping", {
        open = function(conn) conn:ping("hello") end,
        pong = function(conn, payload) conn:send("pong:" .. payload) end,
    })

    -- The server pings at an interval and drops a peer that did not answer the previous ping.
    app:ws("/beat", { close = recorder("beat") }, { pingIntervalMs = 100 })

    -- A message past the limit closes the connection with code 1009.
    app:ws("/small", {
        message = function(conn, data) conn:send(data) end,
        close = recorder("small"),
    }, { maxMessageBytes = 16 })

    app:ws("/queue", {
        message = function(conn, data)
            if data == "exact" then
                local sent = conn:send(string.rep("x", 65536))
                queued.exact = conn.bufferedAmount
                queued.exactSent = sent:await()
                queued.drained = conn.bufferedAmount
                conn:send("after")
                return
            end
            if data == "flood" then
                local first = conn:send(string.rep("a", 32768))
                local second = conn:send(string.rep("b", 32768))
                queued.flood = conn.bufferedAmount
                local third = conn:send(string.rep("c", 32768))
                queued.third = third:await()
                queued.first = first:await()
                return
            end
            if data == "stream" then
                for i = 1, 64 do
                    assert(conn:send(string.rep(string.char(64 + i % 26), 32768)):await(), "An awaited send to a reading peer resolves to true")
                end
                conn:send("streamed")
            end
        end,
        close = recorder("queue"),
    }, { maxQueuedBytes = 65536 })

    local server = app:listen({ host = host, port = 0 })

    -- A ping from the server reaches the client, and the pong of the client reaches the callback.
    local reader = openWs(server.port, "/ping")
    local ping = readFrame(reader)
    assert(ping.opcode == 0x9 and ping.payload == "hello", "The server sends the ping it was asked for")
    reader.conn:send(clientFrame(0xA, "hello")):await()
    local answer = readFrame(reader)
    assert(answer.opcode == 0x1 and answer.payload == "pong:hello", "The pong reaches the callback with its payload")
    reader.conn:close():await()

    -- A peer that answers every ping stays, and one that answers none is dropped as lost.
    local alive = openWs(server.port, "/beat")
    for _ = 1, 3 do
        local beat = readFrame(alive)
        assert(beat.opcode == 0x9, "The server pings at its interval")
        alive.conn:send(clientFrame(0xA, beat.payload)):await()
    end
    local silent = openWs(server.port, "/beat")
    local first = readFrame(silent)
    assert(first.opcode == 0x9, "The silent peer is pinged too")
    assert(readFrame(silent) == nil, "A peer that does not answer its ping is dropped")
    closedRoutes.beat:await()
    assert(closes.beat.code == 1006 and closes.beat.reason:find("did not answer", 1, true), "A dropped peer reaches the callback as 1006 with the reason")
    silent.conn:close():await()
    alive.conn:close():await()

    -- A message at the limit passes, and one past it closes with code 1009.
    local small = openWs(server.port, "/small")
    small.conn:send(clientFrame(0x1, string.rep("s", 16))):await()
    assert(readFrame(small).payload == string.rep("s", 16), "A message at the limit is delivered")
    small.conn:send(clientFrame(0x1, string.rep("s", 17))):await()
    local tooLarge = readFrame(small)
    assert(tooLarge.opcode == 0x8 and closeCode(tooLarge) == 1009, "A message past the limit closes with code 1009")
    closedRoutes.small:await()
    assert(closes.small.code == 1009, "The close callback hears code 1009")
    small.conn:close():await()

    -- A message of exactly the queue limit fits an empty queue, and its send resolves once it left.
    local queue = openWs(server.port, "/queue")
    queue.conn:send(clientFrame(0x1, "exact")):await()
    local exact = readFrame(queue)
    assert(exact and #exact.payload == 65536, "A message of exactly the queue limit is sent whole")
    assert(readFrame(queue).payload == "after", "The connection stays open after a message at the limit")
    assert(queued.exact == 65536, "The queue holds the whole message right after the send, got " .. tostring(queued.exact))
    assert(queued.exactSent == true, "The send resolves to true once the message left")
    assert(queued.drained == 0, "Nothing waits once the message left, got " .. tostring(queued.drained))

    -- Awaited sends follow the peer, so a stream far larger than the queue never passes the limit.
    queue.conn:send(clientFrame(0x1, "stream")):await()
    for i = 1, 64 do
        local part = readFrame(queue)
        assert(part and #part.payload == 32768, "Part " .. i .. " of the stream arrives whole")
    end
    assert(readFrame(queue).payload == "streamed", "Every awaited send went out")

    -- Sends nobody awaits past the limit drop the peer, and the sends that never left resolve to false.
    queue.conn:send(clientFrame(0x1, "flood")):await()
    closedRoutes.queue:await()
    assert(queued.flood == 65536, "Two sends fill the queue, got " .. tostring(queued.flood))
    assert(closes.queue.code == 1006 and closes.queue.reason:find("stopped reading", 1, true), "A peer past the queue limit is dropped as lost")
    assert(queued.third == false and queued.first == false, "The sends that never left resolve to false")
    queue.conn:close():await()

    print("The \"http\" WebSocket limits tests passed.")
end)
