-- Failures of server handlers, routes, WebSocket callbacks and stream callbacks reach the failure handler with their frames, and each still answers its client.
local async = require("async")
local crypto = require("crypto")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"
local serverPort
local appPort

-- The handler takes only the failures this test causes, and raises any other one, such as a failed assertion of the test, so it fails the run instead of being taken.
local failures = {}
async.onFailure(function(err, traceback, frames, kind)
    if type(err) ~= "string" or not err:find("Deliberate", 1, true) then
        error(err, 0)
    end

    failures[#failures + 1] = { err = err, traceback = traceback, frames = frames, kind = kind }
end)

-- Answers the failure whose error holds the text, and fails when none or more than one does.
local function failureWith(text)
    local found = nil
    for _, failure in ipairs(failures) do
        if failure.err:find(text, 1, true) then
            assert(found == nil, "The failure \"" .. text .. "\" should reach the handler once.")
            found = failure
        end
    end

    assert(found ~= nil, "The failure \"" .. text .. "\" should reach the handler.")
    return found
end

-- Asserts that a failure is a failure with the frames of the line of this file that raised it.
local function assertFrames(failure, line)
    assert(failure.kind == "failure", "A handler failure should be reported as a failure, not " .. tostring(failure.kind) .. ".")
    assert(type(failure.traceback) == "string" and failure.traceback:find("stack traceback", 1, true), "The handler should receive a traceback.")
    assert(failure.frames[1].name == "error", "The innermost frame should be the call that raised.")
    assert(failure.frames[2].source:find("http_failures_test.lua", 1, true) and failure.frames[2].line == line, "The next frame should be line " .. line .. ", not " .. tostring(failure.frames[2].line) .. ".")
end

serverPort = http.createServer(function(req, res)
    if req.path == "/sync" then
        error("Deliberate server failure.")
    end

    async.sleep(5):await()
    if req.path == "/late" then
        error("Deliberate late server failure.")
    end

    if req.path == "/open" then
        return
    end

    res:finish("answered")
end):listen({ host = host, port = 0 }).port

local app = http.createApp()
local handled = nil

app:onError(function(_, err)
    handled = err
end)

app:get("/route", function()
    error("Deliberate route failure.")
end)

app:get("/late-route", function()
    async.sleep(5):await()
    error("Deliberate late route failure.")
end)

-- Sends its events apart, so the client receives them as separate chunks.
app:get("/chunks", function(ctx)
    local stream = ctx:sse()
    for index = 1, 3 do
        stream:send("event " .. index)
        async.sleep(20):await()
    end

    stream:close()
end)

app:ws("/socket", {
    open = function(conn)
        conn:send("opened")
    end,
    message = function(conn, data)
        async.sleep(5):await()
        conn:send("echo:" .. data)
        if data == "fail" then
            error("Deliberate message failure.")
        end
    end,
    close = function()
        async.sleep(1):await()
        error("Deliberate close failure.")
    end,
})

app:ws("/refused", {
    open = function()
        error("Deliberate open failure.")
    end,
})

appPort = app:listen({ host = host, port = 0 }).port

local function get(port, path)
    local response, err = http.client.requestRaw({
        url = "http://" .. host .. ":" .. port .. path,
        method = "GET",
        headers = {},
        timeoutSeconds = 10,
    }):await()
    assert(err == nil, err)
    return response
end

-- Builds a masked client text frame, the only framing the server accepts from a client.
local function clientFrame(payload)
    local mask = crypto.randomBytes(4)
    local masked = {}
    for index = 1, #payload do
        masked[index] = string.char((payload:byte(index) ~ mask:byte((index - 1) % 4 + 1)) & 0xFF)
    end

    return string.char(0x81, 0x80 + #payload) .. mask .. table.concat(masked)
end

-- Opens a WebSocket and answers the connected socket and the bytes read past the handshake.
local function openWs(path)
    local conn = socket.tcp.connect(host, appPort):await()
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
        assert(chunk and #chunk > 0, "The handshake closed early.")
        buffer = buffer .. chunk
    end

    assert(buffer:find("101", 1, true), "The handshake did not switch protocols.")
    return conn, buffer:sub(buffer:find("\r\n\r\n", 1, true) + 4)
end

-- Reads one short unmasked server text frame, keeping what follows it for the next read.
local function recvFrame(conn, buffer)
    while #buffer < 2 or #buffer < 2 + (buffer:byte(2) & 0x7F) do
        local chunk = conn:receive(4096):await()
        assert(chunk and #chunk > 0, "The connection closed before a frame arrived.")
        buffer = buffer .. chunk
    end

    local length = buffer:byte(2) & 0x7F
    return buffer:sub(3, 2 + length), buffer:sub(3 + length)
end

async.run(function()
    -- A handler of a server that fails at once or after an await answers 500, and the failure reaches the handler with its frames.
    assert(get(serverPort, "/sync").status == 500, "A handler that fails at once should answer 500.")
    assertFrames(failureWith("Deliberate server failure."), 45)

    assert(get(serverPort, "/late").status == 500, "A handler that fails after an await should answer 500.")
    assertFrames(failureWith("Deliberate late server failure."), 50)

    -- A handler that returns after an await without answering ends its response with 204, and one that answers is untouched.
    assert(get(serverPort, "/open").status == 204, "A handler that returns without answering should end with 204.")
    local answered = get(serverPort, "/answered")
    assert(answered.status == 200 and answered.body == "answered", "A handler that answers after an await should be untouched.")

    -- A route that fails at once or after an await answers 500, runs the error handler and reaches the failure handler.
    assert(get(appPort, "/route").status == 500, "A route that fails should answer 500.")
    assert(handled and handled:find("Deliberate route failure.", 1, true), "The error handler of the app should receive the failure.")
    assertFrames(failureWith("Deliberate route failure."), 68)

    assert(get(appPort, "/late-route").status == 500, "A route that fails after an await should answer 500.")
    assertFrames(failureWith("Deliberate late route failure."), 73)

    -- Every WebSocket callback runs as a task that may await, and its failure reaches the handler.
    local conn, buffer = openWs("/socket")
    local opened
    opened, buffer = recvFrame(conn, buffer)
    assert(opened == "opened", "The callback \"open\" should run.")

    conn:send(clientFrame("hello")):await()
    local echo
    echo, buffer = recvFrame(conn, buffer)
    assert(echo == "echo:hello", "The callback \"message\" should answer after its await, not: " .. tostring(echo))

    conn:send(clientFrame("fail")):await()
    echo, buffer = recvFrame(conn, buffer)
    assert(echo == "echo:fail", "The callback \"message\" should answer before it fails.")
    async.sleep(20):await()
    assertFrames(failureWith("Deliberate message failure."), 95)

    conn:close():await()
    async.sleep(50):await()
    assertFrames(failureWith("Deliberate close failure."), 100)

    local refused = openWs("/refused")
    async.sleep(20):await()
    assertFrames(failureWith("Deliberate open failure."), 106)
    refused:close():await()

    -- A failing callback of a stream reaches the handler once, the stream skips its later callbacks and rejects with the failure.
    local chunks = 0
    local _, chunkErr = http.client.stream({ url = "http://" .. host .. ":" .. appPort .. "/chunks", timeoutSeconds = 10 }, function()
        chunks = chunks + 1
        error("Deliberate chunk failure.")
    end):await()
    assert(chunkErr and chunkErr:find("Deliberate chunk failure.", 1, true), "The stream should reject with the failure of its callback, not: " .. tostring(chunkErr))
    assert(chunks == 1, "The stream should skip the callbacks after a failure.")
    assertFrames(failureWith("Deliberate chunk failure."), 222)

    local _, headErr = http.client.stream({
        url = "http://" .. host .. ":" .. appPort .. "/chunks",
        timeoutSeconds = 10,
        onResponse = function()
            error("Deliberate response failure.")
        end,
    }, function()
        error("The chunks of a stream whose response callback failed should be skipped.")
    end):await()
    assert(headErr and headErr:find("Deliberate response failure.", 1, true), "The stream should reject with the failure of its response callback, not: " .. tostring(headErr))
    assertFrames(failureWith("Deliberate response failure."), 232)

    -- A stream that fails is never reported again as an unobserved rejection.
    http.client.stream({ url = "http://" .. host .. ":" .. appPort .. "/chunks", timeoutSeconds = 10 }, function()
        error("Deliberate unobserved chunk failure.")
    end)
    async.sleep(200):await()
    failureWith("Deliberate unobserved chunk failure.")

    async.onFailure(nil)
    print("The \"http\" failure tests passed.")
end)
