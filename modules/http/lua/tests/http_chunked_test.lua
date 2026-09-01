-- the reactor decodes a Transfer-Encoding: chunked request body, including one carrying trailers, and hands the reassembled body to the handler
local async = require("async")
local http = require("http")
local socket = require("socket")

local host = "127.0.0.1"
local port = 39829

local app = http.createApp()
app:post("/echo", function(ctx)
    ctx:text(ctx.req.body or "")
end)
app:listen({ host = host, port = port })

-- sends a raw request with Connection: close and returns the whole response text
local function send(request)
    local conn = socket.tcp.connect(host, port):await()
    conn:send(request):await()
    local buffer = ""
    while true do
        local chunk, err = conn:receive(4096):await()
        if not chunk or chunk == "" or err then
            break
        end
        buffer = buffer .. chunk
    end
    conn:close():await()
    return buffer
end

local function bodyOf(response)
    local headerEnd = response:find("\r\n\r\n", 1, true)
    assert(headerEnd, "response has no header terminator")
    return response:sub(headerEnd + 4)
end

async.run(function()
    -- a chunked body split across two chunks is decoded and echoed intact
    local chunked = send(table.concat({
        "POST /echo HTTP/1.1",
        "Host: " .. host,
        "Transfer-Encoding: chunked",
        "Content-Type: text/plain",
        "Connection: close",
        "",
        "5\r\nHello\r\n8\r\n, World!\r\n0\r\n\r\n",
    }, "\r\n"))
    assert(chunked:find(" 200 ", 1, true), "a chunked request should be accepted")
    assert(bodyOf(chunked) == "Hello, World!", "the chunked body should decode intact")

    -- a chunked body that carries a trailer after the final chunk still decodes cleanly
    local trailered = send(table.concat({
        "POST /echo HTTP/1.1",
        "Host: " .. host,
        "Transfer-Encoding: chunked",
        "Content-Type: text/plain",
        "Connection: close",
        "",
        "3\r\nabc\r\n0\r\nX-Checksum: 1\r\n\r\n",
    }, "\r\n"))
    assert(bodyOf(trailered) == "abc", "a chunked body with a trailer should decode intact")

    print("http chunked ok")
end)
