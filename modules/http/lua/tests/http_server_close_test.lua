-- A listening server tells its address, closes on demand with or without waiting for the requests in flight, ends its WebSockets with code 1001 and lets the runtime end once it is gone.
local async = require("async")
local crypto = require("crypto")
local fs = require("fs")
local http = require("http")
local platform = require("platform")
local process = require("process")
local socket = require("socket")

local host = "127.0.0.1"

local function connectRefused(port)
    local ok, conn = pcall(function()
        return socket.tcp.connect(host, port):await()
    end)
    if ok and conn then
        conn:close():await()
        return false
    end
    return true
end

local function get(port, path)
    return http.client.requestRaw({ url = "http://" .. host .. ":" .. port .. path, timeoutSeconds = 10 }):await()
end

-- Reads one unmasked server frame from the connection, keeping what follows it in the buffer of the reader.
local function readFrame(reader)
    while true do
        local buffer = reader.buffer
        if #buffer >= 2 then
            local len = buffer:byte(2) & 0x7F
            local offset = 2
            if len == 126 and #buffer >= 4 then
                len = buffer:byte(3) * 256 + buffer:byte(4)
                offset = 4
            end
            if len < 126 and #buffer >= offset + len then
                reader.buffer = buffer:sub(offset + len + 1)
                return { opcode = buffer:byte(1) & 0x0F, payload = buffer:sub(offset + 1, offset + len) }
            end
        end
        local chunk = reader.conn:receive(65536):await()
        assert(chunk and #chunk > 0, "The connection closed before a frame arrived")
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

-- The command shell of Windows drops the outer quote pair of a `/c` string, so a Windows command needs one more around the whole thing.
local function shellCommand(program, argument)
    local quoted = string.format('"%s" "%s"', program, argument)
    if platform.os() == "windows" then
        return '"' .. quoted .. '"'
    end
    return quoted
end

async.run(function()
    -- Port 0 takes a free port, which the server tells along with its host.
    local app = http.createApp()
    app:get("/hello", function(ctx) ctx:text("hello") end)
    local server = app:listen({ host = host, port = 0 })
    assert(server.host == host, "The server tells its host, got " .. tostring(server.host))
    assert(math.type(server.port) == "integer" and server.port > 0, "The server tells the port it bound, got " .. tostring(server.port))
    assert(get(server.port, "/hello").body == "hello", "The server answers on the port it tells")

    -- Collecting the value of the server leaves the server running.
    local port = server.port
    server = nil
    collectgarbage()
    collectgarbage()
    assert(get(port, "/hello").body == "hello", "A collected server value must not close the server")

    -- A plain close ends the server at once and frees its port.
    local plain = http.createServer(function(_, res) res:finish("plain") end):listen({ host = host, port = 0 })
    assert(get(plain.port, "/").body == "plain", "The server of createServer answers")
    assert(plain:close():await() == true, "The close resolves to true")
    assert(connectRefused(plain.port), "A closed server takes no connection")
    assert(plain:close():await() == true, "A second close resolves at once")
    assert(not pcall(plain.close, plain, { gracefull = true }), "An unknown close option is refused")

    -- A graceful close lets the request in flight finish, refuses new connections and resolves once it is done.
    local released, release = async.deferred()
    local slow = http.createApp()
    slow:get("/slow", function(ctx)
        released:await()
        ctx:text("finished")
    end)
    local slowServer = slow:listen({ host = host, port = 0 })
    local inFlight = async.promise(function() return get(slowServer.port, "/slow") end)
    async.sleep(100):await()
    local closing = slowServer:close({ graceful = true, timeoutMs = 5000 })
    async.sleep(50):await()
    assert(not closing:isDone(), "A graceful close waits for the request in flight")
    assert(connectRefused(slowServer.port), "A closing server takes no new connection")
    release()
    assert(closing:await() == true, "The graceful close resolves once the request finished")
    local answered = inFlight:await()
    assert(answered.status == 200 and answered.body == "finished", "The request in flight is answered whole")

    -- A graceful close stops waiting at its deadline and closes what is left.
    local never, keep = async.deferred()
    local stuck = http.createApp()
    stuck:get("/stuck", function() never:await() end)
    local stuckServer = stuck:listen({ host = host, port = 0, requestTimeoutMs = 0 })
    local cut = async.promise(function()
        return pcall(get, stuckServer.port, "/stuck")
    end)
    async.sleep(100):await()
    local started = os.time()
    assert(stuckServer:close({ graceful = true, timeoutMs = 200 }):await() == true, "The deadline of a graceful close resolves it")
    assert(os.time() - started < 5, "The deadline of a graceful close is kept")
    local cutOk, cutRes = cut:await()
    assert(not cutOk or cutRes == nil, "The request cut at the deadline gets no answer")
    assert(keep, "The resolver stays referenced so the stuck route never resumes")

    -- Closing ends a WebSocket with code 1001, which its close callback receives too.
    local closedWith, closeSeen = async.deferred()
    local closeInfo
    local live = http.createApp()
    live:ws("/live", {
        close = function(_, code, reason)
            closeInfo = { code = code, reason = reason }
            closeSeen()
        end,
    })
    local liveServer = live:listen({ host = host, port = 0 })
    local reader = openWs(liveServer.port, "/live")
    assert(liveServer:close():await() == true, "A server with a WebSocket closes")
    local goodbye = readFrame(reader)
    assert(goodbye.opcode == 0x8 and goodbye.payload:sub(1, 2) == string.char(0x03, 0xE9), "The WebSocket is closed with code 1001")
    closedWith:await()
    assert(closeInfo.code == 1001, "The close callback receives code 1001, got " .. tostring(closeInfo.code) .. " " .. tostring(closeInfo.reason))
    reader.conn:close():await()

    -- A closed server no longer keeps the runtime alive, so a script that only closes its server ends by itself.
    if process.available then
        local dir = os.getenv("VARN_TEST_DIR") or "."
        local childPath = dir .. "/server_close_child.lua"
        fs.writeFile(childPath, table.concat({
            'local async = require("async")',
            'local http = require("http")',
            'local server = http.createServer(function(_, res) res:finish("x") end):listen({ host = "127.0.0.1", port = 0 })',
            'async.spawn(function()',
            '    async.sleep(50):await()',
            '    server:close():await()',
            '    print("closed")',
            'end)',
        }, "\n")):await()
        local result = process.exec(shellCommand(arg[-1], childPath), { timeoutMs = 20000 }):await()
        assert(result.code == 0 and result.stdout:find("closed", 1, true), "A runtime whose only server closed must end, got: " .. result.stdout .. result.stderr)
    end

    print("The \"http\" server close tests passed.")
end)
