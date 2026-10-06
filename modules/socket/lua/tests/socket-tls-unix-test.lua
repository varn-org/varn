-- A Unix-domain echo server round-trips a payload and the TLS client surface is exercised.
local async = require("async")
local socket = require("socket")

local path = os.tmpname() .. ".sock"
os.remove(path)

-- Server accepts one client over a Unix socket, echoes a prefixed message, then shuts down.
async.spawn(function()
    local listener, lerr = socket.unix.listen(path, { backlog = 16 }):await()
    assert(not lerr, lerr)

    local client, aerr = listener:accept():await()
    assert(not aerr, aerr)

    local chunk, rerr = client:receive(4096):await()
    assert(not rerr, rerr)
    client:send("echo:" .. chunk):await()

    client:close():await()
    listener:close():await()
end)

-- Client connects to the Unix path, sends, and verifies the echo round-trip.
async.run(function()
    async.sleep(50):await()

    local conn, cerr = socket.unix.connect(path):await()
    assert(not cerr, cerr)

    conn:send("ping"):await()
    local reply, rerr = conn:receive(4096):await()
    assert(not rerr, rerr)
    assert(reply == "echo:ping", "Unexpected reply: " .. tostring(reply))

    conn:close():await()
    os.remove(path)

    -- The TLS surface exists and rejects cleanly against a port with no listener.
    assert(type(socket.tls.connect) == "function", 'The field "socket.tls.connect" must be a function')

    local tlsConn, tlsErr = socket.tls.connect("127.0.0.1", 9, { timeoutMs = 500 }):await()
    assert(not tlsConn, "A TLS connect to a dead port should not resolve")
    assert(tlsErr, "A TLS connect to a dead port should reject with an error")

    print("The \"socket\" TLS and Unix tests passed.")
end)
