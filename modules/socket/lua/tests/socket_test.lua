-- an in-process tcp echo server round-trips a payload back to a client
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port = 9911

-- server accepts one client, echoes a prefixed message, then shuts down
async.spawn(function()
    local listener, lerr = socket.tcp.listen(host, port, 16):await()
    assert(not lerr, lerr)

    local client, aerr = listener:accept():await()
    assert(not aerr, aerr)

    local chunk, rerr = client:receive(4096):await()
    assert(not rerr, rerr)
    client:send("echo:" .. chunk):await()

    client:close():await()
    listener:close():await()
end)

-- client connects once the listener is bound, sends, and verifies the echo
async.run(function()
    async.sleep(50):await()

    local conn, cerr = socket.tcp.connect(host, port):await()
    assert(not cerr, cerr)

    conn:send("ping"):await()
    local reply, rerr = conn:receive(4096):await()
    assert(not rerr, rerr)
    assert(reply == "echo:ping", "unexpected reply: " .. tostring(reply))

    conn:close():await()

    print("socket ok")
end)
