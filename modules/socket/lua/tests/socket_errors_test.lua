-- socket error paths the happy-path suites skip, namely udp port range and a missing unix socket
local async = require("async")
local socket = require("socket")

-- udp.bind rejects a port outside 1..65535 (validated before the bind is attempted)
assert(not pcall(function() return socket.udp.bind("127.0.0.1", 0) end), "udp.bind should reject port 0")
assert(not pcall(function() return socket.udp.bind("127.0.0.1", 70000) end), "udp.bind should reject a port above 65535")

async.run(function()
    -- connecting to a unix socket path that does not exist rejects with an error
    local conn, err = socket.unix.connect("/tmp/varn_no_such_socket_xyz_12345.sock"):await()
    assert(not conn and err, "connecting to a missing unix socket should reject")
    print("socket errors ok")
end)
