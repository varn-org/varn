-- The writes of a handler stream as they are made: chunked unless the handler declared a length, each one a promise that resolves once it left for the client and rejects once the client is gone, so a writer that awaits never runs ahead of its client.
local async = require("async")
local datetime = require("datetime")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"
local port
local plainPort

local slice = string.rep("v", 256 * 1024)
local floodSlices = 256
local flood = { written = 0, done = false, error = nil }
local gone = { error = nil }
local refusals = {}
local failures = 0

-- Only the failure the stream route raises on purpose is expected, and any other one fails the test.
async.onFailure(function(err, traceback)
    if tostring(err):find("The stream failed.", 1, true) then
        failures = failures + 1
        return
    end
    print(tostring(err) .. "\n" .. tostring(traceback))
    os.exit(1)
end)

local app = http.createApp()

app:get("/paced", function(ctx)
    ctx:type("text/plain")
    ctx:write("first\n"):await()
    async.sleep(300):await()
    ctx:write("second\n"):await()
    async.sleep(300):await()
    ctx:send("last\n")
end)

app:get("/declared", function(ctx)
    ctx:header("Content-Length", "10")
    ctx:write("12345"):await()
    local _, err = ctx:write("678901"):await()
    refusals.past = err
    ctx:write("67890"):await()
end)

app:get("/ended", function(ctx)
    ctx:send("done")
    local _, err = ctx:write("late"):await()
    refusals.ended = err
end)

app:get("/fails", function(ctx)
    ctx:write("partial"):await()
    error("The stream failed.")
end)

app:get("/flood", function(ctx)
    for _ = 1, floodSlices do
        local ok, err = ctx:write(slice):await()
        if not ok then
            flood.error = err
            return
        end
        flood.written = flood.written + 1
    end
    flood.done = true
end)

app:get("/gone", function(ctx)
    while true do
        local ok, err = ctx:write(slice):await()
        if not ok then
            gone.error = err
            return
        end
    end
end)

port = app:listen({ host = host, port = 0, compress = false }).port

plainPort = http.createServer(function(req, res)
    res:status(201)
    res:write("alpha"):await()
    async.sleep(50):await()
    res:write("beta"):await()
    res:finish("gamma")
end):listen({ host = host, port = 0, compress = false }).port

local function nowMs()
    return datetime.now():millis()
end

local function open(targetPort, path)
    local conn = socket.tcp.connect(host, targetPort):await()
    conn:send("GET " .. path .. " HTTP/1.1\r\nHost: " .. host .. "\r\nConnection: close\r\n\r\n"):await()
    return conn
end

-- Reads until the peer closes and notes when each marker first appeared.
local function readAll(conn, markers)
    local started = nowMs()
    local buffer = ""
    local seen = {}
    while true do
        local chunk = conn:receive(65536):await()
        if not chunk or #chunk == 0 then
            break
        end
        buffer = buffer .. chunk
        for _, marker in ipairs(markers or {}) do
            if not seen[marker] and buffer:find(marker, 1, true) then
                seen[marker] = nowMs() - started
            end
        end
    end
    conn:close():await()
    return buffer, seen
end

-- Reads until the peer closes and answers how many bytes arrived, without keeping them.
local function countAll(conn)
    local total = 0
    while true do
        local chunk = conn:receive(65536):await()
        if not chunk or #chunk == 0 then
            break
        end
        total = total + #chunk
    end
    conn:close():await()
    return total
end

local function split(response)
    local headerEnd = response:find("\r\n\r\n", 1, true)
    assert(headerEnd, "The response has no header terminator")
    return response:sub(1, headerEnd + 1):lower(), response:sub(headerEnd + 4)
end

local function waitFor(predicate, timeoutMs)
    local deadline = nowMs() + timeoutMs
    while not predicate() and nowMs() < deadline do
        async.sleep(10):await()
    end
    return predicate()
end

async.run(function()
    -- Each write reaches the client as its own chunk when it is made, not when the handler ends.
    local paced, seen = readAll(open(port, "/paced"), { "6\r\nfirst\n\r\n", "7\r\nsecond\n\r\n", "5\r\nlast\n\r\n0\r\n\r\n" })
    local head, body = split(paced)
    assert(head:find("transfer-encoding: chunked", 1, true), "A streamed response should be chunked")
    assert(not head:find("content-length", 1, true), "A chunked response should carry no length")
    assert(body == "6\r\nfirst\n\r\n7\r\nsecond\n\r\n5\r\nlast\n\r\n0\r\n\r\n", "The chunks should arrive as written: " .. body)
    assert(seen["6\r\nfirst\n\r\n"] < 250, "The first write should arrive before the handler sleeps")
    assert(seen["7\r\nsecond\n\r\n"] >= 250, "The second write should arrive after the first sleep")
    assert(seen["5\r\nlast\n\r\n0\r\n\r\n"] >= 550, "The end should arrive after the second sleep")

    -- A length the handler declares frames the stream, and a write past it is refused while the rest goes out.
    local declared = readAll(open(port, "/declared"))
    head, body = split(declared)
    assert(head:find("content-length: 10", 1, true), "The declared length should frame the stream")
    assert(not head:find("transfer-encoding", 1, true), "A declared length leaves out chunked encoding")
    assert(body == "1234567890", "The body should hold the writes that fit: " .. body)
    assert(refusals.past and refusals.past:find("Content-Length", 1, true), "A write past the declared length should reject")

    -- A write after the response ended rejects.
    local ended = readAll(open(port, "/ended"))
    head, body = split(ended)
    assert(body == "done", "The sent body should stand alone")
    assert(waitFor(function() return refusals.ended ~= nil end, 1000), "A write after the end should settle")
    assert(refusals.ended:find("already ended", 1, true), "A write after the end should reject")

    -- A handler that fails mid-stream closes the connection after what it wrote, without the end of the stream.
    local failed = readAll(open(port, "/fails"))
    head, body = split(failed)
    assert(head:find("^http/1.1 200"), "The head of a failed stream already left with its status")
    assert(body == "7\r\npartial\r\n", "A failed stream should stop after its writes, without its end: " .. body)
    assert(waitFor(function() return failures == 1 end, 1000), "The failure should reach the failure handler")

    -- The response of a server built with "createServer" streams its writes the same way.
    local plain = readAll(open(plainPort, "/"))
    head, body = split(plain)
    assert(head:find("^http/1.1 201"), "The status set before the first write should lead the stream")
    assert(head:find("transfer-encoding: chunked", 1, true), "The writes of a server response should be chunked")
    assert(body == "5\r\nalpha\r\n4\r\nbeta\r\n5\r\ngamma\r\n0\r\n\r\n", "The writes should arrive in order: " .. body)

    -- A writer that awaits stops while the client does not read, and resumes once it does.
    local conn = open(port, "/flood")
    assert(#conn:receive(1024):await() > 0, "The flood should start")
    async.sleep(500):await()
    local heldAt = flood.written
    assert(not flood.done, "A writer that awaits should not finish while the client does not read")
    assert(heldAt * #slice < floodSlices * #slice / 2, "The writer should be held near what the connection buffers, not " .. heldAt .. " slices")
    async.sleep(300):await()
    assert(flood.written == heldAt, "The writer should stay held while nothing is read")
    local rest = countAll(conn)
    assert(waitFor(function() return flood.done end, 5000), "The writer should finish once the client reads")
    assert(rest > (floodSlices - heldAt - 2) * #slice, "The rest of the stream should arrive")

    -- A write to a client that left rejects, so the writer stops.
    local leaving = open(port, "/gone")
    assert(#leaving:receive(65536):await() > 0, "The stream should start")
    leaving:close():await()
    assert(waitFor(function() return gone.error ~= nil end, 5000), "A write to a client that left should reject")
    assert(gone.error:find("closed the connection", 1, true), "The rejection should say the client left: " .. gone.error)

    print("The \"http\" stream write tests passed.")
    os.exit(0)
end)
