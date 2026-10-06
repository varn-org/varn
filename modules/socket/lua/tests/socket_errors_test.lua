-- Socket error paths the happy-path suites skip, namely the UDP port range, a missing Unix socket and a host that does not exist.
local async = require("async")
local socket = require("socket")

-- A bind through `udp.bind` rejects a port outside `0..65535`, validated before the bind is attempted.
assert(not pcall(function() return socket.udp.bind("127.0.0.1", -1) end), 'The call "udp.bind" should reject a negative port')
assert(not pcall(function() return socket.udp.bind("127.0.0.1", 70000) end), 'The call "udp.bind" should reject a port above 65535')

async.run(function()
    -- Connecting to a Unix socket path that does not exist rejects with an error.
    local conn, err = socket.unix.connect("/tmp/varn_no_such_socket_xyz_12345.sock"):await()
    assert(not conn and err and err:find("The connection to /tmp/varn_no_such_socket_xyz_12345.sock failed", 1, true), "Connecting to a missing Unix socket should reject with its path, not: " .. tostring(err))

    -- A host that does not exist rejects with its name, looked up off the loop thread.
    local named, nerr = socket.tcp.connect("host.invalid", 80):await()
    assert(not named and nerr == 'The host "host.invalid" could not be resolved.', "An unknown host should reject with its name, not: " .. tostring(nerr))
    print("The \"socket\" errors tests passed.")
end)
