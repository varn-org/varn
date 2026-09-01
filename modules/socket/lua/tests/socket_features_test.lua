-- exercises a tcp connect/listen/accept/send/receive/close round-trip and a udp bind/sendTo/recvFrom round-trip with defaults
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local tcpPort = 9811
local udpServerPort = 9821
local udpClientPort = 9822

-- tcp server accepts one client, echoes a prefixed reply, then shuts down cleanly
async.spawn(function()
    -- listens without an explicit backlog to exercise the default of 64
    local listener, lerr = socket.tcp.listen(host, tcpPort):await()
    assert(not lerr, lerr)

    local client, aerr = listener:accept():await()
    assert(not aerr, aerr)

    -- receives without an explicit maxBytes to exercise the default of 65536
    local chunk, rerr = client:receive():await()
    assert(not rerr, rerr)
    client:send("echo:" .. chunk):await()

    client:close():await()
    listener:close():await()
end)

-- udp server binds, echoes one datagram back to its sender, then closes
async.spawn(function()
    local server, berr = socket.udp.bind(host, udpServerPort):await()
    assert(not berr, berr)

    local packet, rerr = server:recvFrom():await()
    assert(not rerr, rerr)
    server:sendTo(packet.host, packet.port, "echo:" .. packet.data):await()

    server:close():await()
end)

-- client drives both round-trips to completion and verifies the replies and the recvFrom shape
async.run(function()
    async.sleep(80):await()

    -- tcp round-trip
    local conn, cerr = socket.tcp.connect(host, tcpPort):await()
    assert(not cerr, cerr)
    conn:send("ping"):await()
    local reply, rerr = conn:receive():await()
    assert(not rerr, rerr)
    assert(reply == "echo:ping", "unexpected tcp reply: " .. tostring(reply))
    conn:close():await()

    -- udp round-trip with the recvFrom result table
    local udp, uerr = socket.udp.bind(host, udpClientPort):await()
    assert(not uerr, uerr)
    udp:sendTo(host, udpServerPort, "ping"):await()
    local datagram, derr = udp:recvFrom():await()
    assert(not derr, derr)
    assert(type(datagram) == "table", "recvFrom should resolve to a table")
    assert(datagram.data == "echo:ping", "unexpected udp reply: " .. tostring(datagram.data))
    assert(datagram.host == host, "unexpected sender host: " .. tostring(datagram.host))
    assert(datagram.port == udpServerPort, "unexpected sender port: " .. tostring(datagram.port))
    udp:close():await()

    -- a listener with no incoming connection releases the pending accept on close
    local idle, ierr = socket.tcp.listen(host, 9831):await()
    assert(not ierr, ierr)
    async.spawn(function()
        async.sleep(40):await()
        idle:close():await()
    end)
    local accepted, aerr = idle:accept():await()
    assert(accepted == nil, "accept on a closed listener should not resolve to a socket")
    assert(aerr, "accept released by close should report an error")

    print("socket features ok")
end)
