-- An open handler that registers more routes moves every route in memory, and the connection it opened still reaches its own message and close handlers.
local async = require("async")
local crypto = require("crypto")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"

local function clientFrame(opcode, payload)
    local key = crypto.randomBytes(4)
    local out = {}
    for i = 1, #payload do
        out[i] = string.char(payload:byte(i) ~ key:byte((i - 1) % 4 + 1))
    end
    return string.char(0x80 | opcode, 0x80 | #payload) .. key .. table.concat(out)
end

local function readFrame(reader)
    while true do
        local buffer = reader.buffer
        if #buffer >= 2 then
            local len = buffer:byte(2) & 0x7F
            if #buffer >= 2 + len then
                reader.buffer = buffer:sub(3 + len)
                return { opcode = buffer:byte(1) & 0x0F, payload = buffer:sub(3, 2 + len) }
            end
        end

        local chunk = reader.conn:receive(65536):await()
        if not chunk or #chunk == 0 then
            return nil
        end

        reader.buffer = reader.buffer .. chunk
    end
end

async.run(function()
    local closeCode
    local closed, closeSeen = async.deferred()
    local app = http.createApp()
    app:ws("/first", {
        open = function()
            for index = 1, 64 do
                app:ws("/added" .. index, { message = function() end })
            end
        end,
        message = function(conn, data)
            conn:send("first:" .. data)
        end,
        close = function(_, code)
            closeCode = code
            closeSeen()
        end,
    })
    local server = app:listen({ host = host, port = 0 })

    local conn = socket.tcp.connect(host, server.port):await()
    conn:send(table.concat({
        "GET /first HTTP/1.1",
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

    assert(buffer:match("^HTTP/1.1 101"), "The upgrade was refused: " .. buffer)
    local reader = { conn = conn, buffer = buffer:sub(buffer:find("\r\n\r\n", 1, true) + 4) }

    conn:send(clientFrame(0x1, "hi")):await()
    local reply = readFrame(reader)
    assert(reply and reply.payload == "first:hi", "The message reached another handler: " .. tostring(reply and reply.payload))

    conn:send(clientFrame(0x8, string.char(0x03, 0xE8))):await()
    closed:await()
    assert(closeCode == 1000, "The close reached another handler with the code " .. tostring(closeCode))

    conn:close():await()
    server:close():await()
    print("The \"http\" WebSocket route growth test passed.")
end)
