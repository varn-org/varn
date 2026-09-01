-- closing a socket releases a pending receive and a pending accept
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port = 9861

async.run(function()
    -- a listener with no incoming connection keeps accept pending until closed
    local listener = socket.tcp.listen(host, port, 16):await()
    async.spawn(function()
        async.sleep(80):await()
        listener:close():await()
    end)
    local accepted, aerr = listener:accept():await()
    print("accept after close:", accepted, aerr)

    -- a connected socket with an idle peer keeps receive pending until closed
    local server = socket.tcp.listen(host, port + 1, 16):await()
    local conn = socket.tcp.connect(host, port + 1):await()
    local peer = server:accept():await()
    async.spawn(function()
        async.sleep(80):await()
        conn:close():await()
    end)
    local chunk, rerr = conn:receive(4096):await()
    print("receive after close:", chunk and #chunk or chunk, rerr)
    peer:close():await()
    server:close():await()

    print("socket close pending ok")
end)
