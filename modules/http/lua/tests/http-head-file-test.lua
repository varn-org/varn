-- A HEAD request answers the head of a file alone, from a route and from the static folder, so the next response on the same connection starts where the head ends.
local async = require("async")
local fs = require("fs")
local http = require("http")
local socket = require("socket")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set.')
local host = "127.0.0.1"

local function exchange(port, path)
    local conn = socket.tcp.connect(host, port):await()
    conn:send("HEAD " .. path .. " HTTP/1.1\r\nHost: x\r\n\r\nGET " .. path .. " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"):await()

    local parts = {}
    while true do
        local chunk = conn:receive(65536):await()
        if not chunk or chunk == "" then
            break
        end

        parts[#parts + 1] = chunk
    end

    conn:close():await()
    return table.concat(parts)
end

async.run(function()
    local body = string.rep("b", 1000)
    fs.mkdir(dir .. "/public"):await()
    fs.writeFile(dir .. "/public/file.txt", body):await()

    local app = http.createApp()
    app:get("/route", function(ctx)
        ctx:file(dir .. "/public/file.txt")
    end)
    local server = app:listen({ host = host, port = 0, publicDir = dir .. "/public" })

    for _, path in ipairs({ "/route", "/file.txt" }) do
        local answer = exchange(server.port, path)
        local headEnd = answer:find("\r\n\r\n", 1, true)
        assert(headEnd, "The HEAD response has no head for " .. path)
        assert(answer:sub(headEnd + 4, headEnd + 11) == "HTTP/1.1", "The HEAD response for " .. path .. " carried a body")
        assert(answer:sub(-#body) == body, "The GET after the HEAD for " .. path .. " lost its body")
    end

    server:close():await()
    print("The \"http\" HEAD file test passed.")
end)
