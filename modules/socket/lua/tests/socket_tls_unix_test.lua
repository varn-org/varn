-- a unix-domain echo server round-trips a payload and the tls client surface is exercised
local async = require("async")
local socket = require("socket")

local path = os.tmpname() .. ".sock"
os.remove(path)

-- server accepts one client over a unix socket, echoes a prefixed message, then shuts down
async.spawn(function()
    local listener, lerr = socket.unix.listen(path, 16):await()
    assert(not lerr, lerr)

    local client, aerr = listener:accept():await()
    assert(not aerr, aerr)

    local chunk, rerr = client:receive(4096):await()
    assert(not rerr, rerr)
    client:send("echo:" .. chunk):await()

    client:close():await()
    listener:close():await()
end)

-- client connects to the unix path, sends, and verifies the echo round-trip
async.run(function()
    async.sleep(50):await()

    local conn, cerr = socket.unix.connect(path):await()
    assert(not cerr, cerr)

    conn:send("ping"):await()
    local reply, rerr = conn:receive(4096):await()
    assert(not rerr, rerr)
    assert(reply == "echo:ping", "unexpected reply: " .. tostring(reply))

    conn:close():await()
    os.remove(path)

    -- tls surface exists and rejects cleanly against a port with no listener
    assert(type(socket.tls.connect) == "function", "socket.tls.connect must be a function")

    local tlsConn, tlsErr = socket.tls.connect("127.0.0.1", 9, { timeoutMs = 500 }):await()
    assert(not tlsConn, "tls connect to a dead port should not resolve")
    assert(tlsErr, "tls connect to a dead port should reject with an error")

    print("socket tls/unix ok")
end)
