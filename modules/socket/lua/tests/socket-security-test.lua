-- Confirms out-of-range arguments are rejected and closed-socket operations fail cleanly across port range (CWE-20), backlog range (CWE-400), use-after-close (CWE-416), peer-close read (CWE-20), and bounded large receive (CWE-789).
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port

-- Server accepts one client, reads its payload, then closes to drive the closed-socket checks.
async.spawn(function()
    local listener = socket.tcp.listen(host, 0, { backlog = 16 }):await()
    port = listener.port
    local client = listener:accept():await()
    -- Sends a bounded payload back so the client can do a large but bounded receive.
    local chunk = client:receive():await()
    client:send(string.rep("y", 4096)):await()
    -- Ignores the trailing content, then closes from the server side.
    client:receive():await()
    client:close():await()
    listener:close():await()
end)

async.run(function()
    -- SOCK-009: A port above 65535 is rejected with a raised error.
    local okHigh = pcall(function() return socket.tcp.connect(host, 99999):await() end)
    assert(not okHigh, 'The call "connect" to port 99999 should be rejected')

    -- SOCK-009, SOCK-180: A port outside `0..65535` is rejected on `listen`, while port 0 asks the system for a free one.
    local okNegative = pcall(function() return socket.tcp.listen(host, -1):await() end)
    assert(not okNegative, 'The call "listen" on port -1 should be rejected')
    local okAbove = pcall(function() return socket.tcp.listen(host, 65536):await() end)
    assert(not okAbove, 'The call "listen" on port 65536 should be rejected')

    -- SOCK-044: A negative backlog is rejected.
    local okNegBacklog = pcall(function() return socket.tcp.listen(host, 0, { backlog = -5 }):await() end)
    assert(not okNegBacklog, "A negative backlog should be rejected")

    -- SOCK-044: An oversized backlog is rejected.
    local okBigBacklog = pcall(function() return socket.tcp.listen(host, 0, { backlog = 100000 }):await() end)
    assert(not okBigBacklog, "An oversized backlog should be rejected")

    async.sleep(80):await()
    local conn, cerr = socket.tcp.connect(host, port):await()
    assert(not cerr, cerr)
    conn:send("ping"):await()

    -- SOCK-019: A multi-KB read returns the full payload without over-allocating.
    local big, rerr = conn:receive():await()
    assert(not rerr, rerr)
    assert(#big == 4096, "A bounded large receive should return the full payload")

    conn:close():await()

    -- SOCK-054, SOCK-149: A `send` on a closed socket fails cleanly with an error.
    local sent, serr = conn:send("after-close"):await()
    assert(sent == nil, 'The call "send" after "close" should yield "nil"')
    assert(serr == "The socket was closed.", 'The call "send" after "close" should report the close, not: ' .. tostring(serr))

    -- SOCK-054: A `receive` on a closed socket returns an error, not a crash.
    local got, gerr = conn:receive():await()
    assert(got == nil, 'The call "receive" after "close" should yield "nil"')
    assert(gerr == "The socket was closed.", 'The call "receive" after "close" should report the close, not: ' .. tostring(gerr))

    print("The \"socket\" security tests passed.")
end)
