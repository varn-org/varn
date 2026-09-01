-- an explicit connect timeout leaves a normal connect untouched and bounds a connect that never answers
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port = 9913

-- a live listener proves the timeout argument does not disturb a successful connect
async.spawn(function()
    local listener, lerr = socket.tcp.listen(host, port, 16):await()
    assert(not lerr, lerr)

    local client = listener:accept():await()
    client:close():await()
    listener:close():await()
end)

async.run(function()
    async.sleep(50):await()

    -- a generous timeout still connects to the live listener
    local conn, cerr = socket.tcp.connect(host, port, 2000):await()
    assert(not cerr, cerr)
    conn:close():await()

    -- a connect to a reserved test-net address that never answers settles via the timeout instead of hanging
    local dead, derr = socket.tcp.connect("192.0.2.1", 80, 300):await()
    assert(dead == nil, "the connect to a dead address should not succeed")
    assert(derr, "the connect to a dead address should reject")

    print("socket timeout ok")
end)
