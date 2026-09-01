-- an in-process tcp server echoes one message back to a client and both shut down
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port = 9831

async.spawn(function()
    local listener = socket.tcp.listen(host, port, 16):await()
    local client = listener:accept():await()
    local chunk = client:receive(4096):await()
    client:send("echo:" .. chunk):await()
    client:close():await()
    listener:close():await()
end)

async.run(function()
    async.sleep(80):await()

    local conn = socket.tcp.connect(host, port):await()
    conn:send("hello"):await()
    local reply = conn:receive(4096):await()
    print("tcp reply:", reply)
    conn:close():await()

    print("socket tcp round-trip ok")
end)
