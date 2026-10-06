-- A handler past `requestTimeoutMs` is answered with 504, a body the head declares too large is answered with 413 before it is read, `maxRequestBodyBytes` sets that limit, and static files are served only from a folder the options name.
local async = require("async")
local fs = require("fs")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"
local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set. Run the tests with "python3 varn.py test".')

local function request(port, method, path, body)
    return http.client.requestRaw({
        url = "http://" .. host .. ":" .. port .. path,
        method = method,
        body = body,
        headers = body and { ["Content-Length"] = tostring(#body) } or {},
        timeoutSeconds = 10,
    }):await()
end

-- Sends raw bytes and reads until the server closes or the head of an answer arrived, and answers what was read.
local function exchange(port, bytes, untilClosed)
    local conn = socket.tcp.connect(host, port):await()
    conn:send(bytes):await()
    local buffer = ""
    while untilClosed or not buffer:find("\r\n\r\n", 1, true) do
        local chunk = conn:receive(65536):await()
        if not chunk or #chunk == 0 then
            break
        end
        buffer = buffer .. chunk
    end
    conn:close():await()
    return buffer
end

async.run(function()
    -- A handler that runs past the deadline is answered with 504, and what it sends later is dropped.
    local lateSends = 0
    local app = http.createApp()
    app:get("/slow", function(ctx)
        async.sleep(600):await()
        lateSends = lateSends + 1
        ctx:text("too late")
    end)
    app:get("/fast", function(ctx) ctx:text("fast") end)
    local server = app:listen({ host = host, port = 0, requestTimeoutMs = 200 })

    local timedOut = request(server.port, "GET", "/slow")
    assert(timedOut.status == 504, "A handler past the deadline is answered with 504, got " .. tostring(timedOut.status))
    assert(request(server.port, "GET", "/fast").body == "fast", "A handler within the deadline answers")
    async.sleep(700):await()
    assert(lateSends == 1, "The handler keeps running after the deadline")

    -- The bare server keeps the same deadline.
    local bare = http.createServer(function(_, res)
        async.sleep(600):await()
        res:finish("too late")
    end):listen({ host = host, port = 0, requestTimeoutMs = 200 })
    assert(request(bare.port, "GET", "/").status == 504, "The bare server answers 504 past its deadline")

    -- A body the head declares too large is answered at once, before the client sent any of it, and the connection closes.
    local limited = http.createApp()
    limited:post("/up", function(ctx) ctx:text("stored " .. #ctx.req.body) end)
    local limitedServer = limited:listen({ host = host, port = 0, maxRequestBodyBytes = 1024 })
    local head = exchange(limitedServer.port, "POST /up HTTP/1.1\r\nHost: " .. host .. "\r\nContent-Length: 104857600\r\n\r\n", false)
    assert(head:find("^HTTP/1.1 413"), "A declared body over the limit is answered with 413 before it is sent, got: " .. head)
    assert(head:find("Connection: close", 1, true), "The answer to a body too large closes the connection")

    -- The client that sends nothing more is let go within a short bound.
    local closedAfter = os.time()
    local whole = exchange(limitedServer.port, "POST /up HTTP/1.1\r\nHost: " .. host .. "\r\nContent-Length: 104857600\r\n\r\n", true)
    assert(whole:find("^HTTP/1.1 413"), "The answer arrives before the close")
    assert(os.time() - closedAfter <= 5, "The server stops waiting for the rest of the body within a short bound")

    -- A chunk that would pass the limit is refused the same way.
    local chunked = exchange(limitedServer.port, "POST /up HTTP/1.1\r\nHost: " .. host .. "\r\nTransfer-Encoding: chunked\r\n\r\n100000\r\n", false)
    assert(chunked:find("^HTTP/1.1 413"), "A chunk over the limit is answered with 413, got: " .. chunked)

    -- A body within the limit is read, and one just over it is refused through the client.
    assert(request(limitedServer.port, "POST", "/up", string.rep("a", 1024)).body == "stored 1024", "A body at the limit is read")
    assert(request(limitedServer.port, "POST", "/up", string.rep("a", 1025)).status == 413, "A body one byte over the limit is refused")

    -- No folder is served unless the options name one.
    fs.writeFile(dir .. "/page.txt", "from disk"):await()
    local plain = http.createApp()
    plain:get("/page.txt", function(ctx) ctx:text("from the route") end)
    local plainServer = plain:listen({ host = host, port = 0 })
    assert(request(plainServer.port, "GET", "/page.txt").body == "from the route", "Without a folder no file is served")

    local files = http.createServer(function(_, res) res:finish("from the handler") end):listen({ host = host, port = 0, publicDir = dir })
    assert(request(files.port, "GET", "/page.txt").body == "from disk", "Naming a folder serves its files")
    assert(request(files.port, "GET", "/missing.txt").body == "from the handler", "A path without a file reaches the handler")

    local off = http.createServer(function(_, res) res:finish("from the handler") end):listen({ host = host, port = 0, publicDir = dir, servePublic = false })
    assert(request(off.port, "GET", "/page.txt").body == "from the handler", "A folder turned off is not served")

    print("The \"http\" request limits tests passed.")
end)
