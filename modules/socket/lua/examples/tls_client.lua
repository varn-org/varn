-- opens a verified tls connection and speaks a minimal http request over it
local async = require("async")
local socket = require("socket")

local host = os.getenv("VARN_TLS_HOST") or "example.com"
local port = tonumber(os.getenv("VARN_TLS_PORT") or "443")

async.run(function()
    local conn, cerr = socket.tls.connect(host, port, { timeoutMs = 5000 }):await()
    if cerr then
        print("tls connect error:", cerr)
        return
    end

    local request = "GET / HTTP/1.0\r\nHost: " .. host .. "\r\nConnection: close\r\n\r\n"
    conn:send(request):await()

    local reply, rerr = conn:receive(512):await()
    if rerr then
        print("tls receive error:", rerr)
    else
        local statusLine = reply:match("^[^\r\n]*")
        print("tls status:", statusLine)
    end

    conn:close():await()

    print("socket tls client ok")
end)
