-- a HEAD request to a route handler advertises Content-Length but sends no body, so keep-alive framing stays in sync
local async = require("async")
local http = require("http")
local socket = require("socket")

local port = 3992

http.createServer(function(req, res)
    res:setHeader("Content-Type", "text/plain")
    res:finish("Hello, World!")
end):listen({ host = "127.0.0.1", port = port, servePublic = false })

async.run(function()
    async.sleep(50):await()

    local conn, cerr = socket.tcp.connect("127.0.0.1", port):await()
    assert(not cerr, cerr)

    conn:send("HEAD / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"):await()

    local resp = ""
    while true do
        local chunk, rerr = conn:receive(4096):await()
        assert(not rerr, rerr)
        if chunk == "" then
            break
        end
        resp = resp .. chunk
    end
    conn:close():await()

    -- HEAD still carries the Content-Length the GET body would have had
    assert(resp:find("Content%-Length: 13"), "HEAD must advertise Content-Length: " .. resp)

    -- but the response must end at the header terminator with zero body bytes
    local headEnd = assert(resp:find("\r\n\r\n", 1, true), "missing header terminator")
    local extra = #resp - (headEnd + 3)
    assert(extra == 0, "HEAD leaked a body of " .. extra .. " bytes")

    print("http head ok")
end)
