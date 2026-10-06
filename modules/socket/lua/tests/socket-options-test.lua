-- Socket options, addresses, half close, the bound on queued sends, UDP broadcast and multicast membership, and errors that name their address.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"

async.run(function()
    local listener = socket.tcp.listen(host, 0):await()
    local conn = socket.tcp.connect(host, listener.port):await()
    local peer = listener:accept():await()

    -- Both ends report their own address and the address of the other end.
    assert(conn.localHost == host and conn.peerHost == host, "The client should report both hosts")
    assert(conn.peerPort == listener.port, "The client should report the port of the listener, not: " .. tostring(conn.peerPort))
    assert(peer.localPort == listener.port and peer.peerPort == conn.localPort, "The accepted socket should mirror the client")
    assert(conn.pendingBytes == 0, "A socket with no send queues nothing")
    assert(type(conn.send) == "function", "The methods should stay reachable next to the fields")

    -- The options resolve once they are set.
    assert(conn:setNoDelay(true):await() == "ok", "The method \"setNoDelay\" should resolve")
    assert(conn:setNoDelay(false):await() == "ok", "The method \"setNoDelay\" should resolve when it turns the option off")
    assert(conn:setKeepAlive(true):await() == "ok", "The method \"setKeepAlive\" should resolve")
    assert(not pcall(conn.setNoDelay, conn, 1), "The method \"setNoDelay\" should take only a boolean")

    -- Shutting the sending side down lets the peer read the end of the stream while the socket still receives.
    conn:send("last"):await()
    assert(conn:shutdown("send"):await() == "ok", "The method \"shutdown\" should resolve")
    assert(peer:receive():await() == "last", "The data sent before the shutdown should arrive")
    assert(peer:receive():await() == "", "The peer should read the end of the stream after the shutdown")
    local _, afterShutdown = conn:send("more"):await()
    assert(afterShutdown == "The socket was shut down for sending.", "A send after the shutdown should be refused, not: " .. tostring(afterShutdown))
    peer:send("still open"):await()
    assert(conn:receive():await() == "still open", "The socket should still receive after its shutdown")
    assert(not pcall(conn.shutdown, conn, "both"), "The method \"shutdown\" should take only \"send\"")
    conn:close():await()
    peer:close():await()

    -- A send that would queue past `maxQueuedBytes` is refused while the queued ones still leave.
    local bounded = socket.tcp.connect(host, listener.port, { maxQueuedBytes = 4 * 1024 * 1024 }):await()
    local reader = listener:accept():await()
    local chunk = string.rep("y", 1024 * 1024)
    local sends = {}
    for i = 1, 6 do
        sends[i] = bounded:send(chunk)
    end

    assert(bounded.pendingBytes == 4 * 1024 * 1024, "The queued sends should count every byte not yet sent, not: " .. tostring(bounded.pendingBytes))
    for i = 5, 6 do
        local _, refused = sends[i]:await()
        assert(refused == "The socket already queues 4194304 bytes, so a send of 1048576 bytes would pass its limit of 4194304 bytes.", "A send past the bound should be refused, not: " .. tostring(refused))
    end
    local readTotal = 0
    while readTotal < 4 * 1024 * 1024 do
        readTotal = readTotal + #reader:receive():await()
    end

    for i = 1, 4 do
        assert(sends[i]:await() == "ok", "The sends within the bound should leave")
    end

    assert(bounded.pendingBytes == 0, "Every queued byte should have left, not: " .. tostring(bounded.pendingBytes))
    bounded:close():await()
    reader:close():await()

    -- A refused connect names the address it tried.
    local port = listener.port
    listener:close():await()
    local refusedConn, refusedErr = socket.tcp.connect(host, port):await()
    assert(refusedConn == nil and refusedErr == "The connection to " .. host .. ":" .. port .. " was refused.", "A refused connect should name its address, not: " .. tostring(refusedErr))

    -- A UDP socket sends to the broadcast address only once broadcast is on.
    local udp = socket.udp.bind("0.0.0.0", 0):await()
    local _, denied = udp:sendTo("255.255.255.255", udp.port, "nobody"):await()
    assert(denied and denied:find("The datagram to 255.255.255.255:" .. udp.port .. " could not be sent", 1, true), "A broadcast without the option should be refused with its address, not: " .. tostring(denied))
    assert(udp:setBroadcast(true):await() == "ok", "The method \"setBroadcast\" should resolve")
    -- The option lifts only the refusal for want of permission, so a network without a broadcast route, as on hosted runners, refuses the datagram the same way with it on.
    local permission = denied:find("(system error 13)", 1, true) or denied:find("(system error 10013)", 1, true)
    local sent, routeErr = udp:sendTo("255.255.255.255", udp.port, "everybody"):await()
    assert(not permission or sent == "ok" or routeErr ~= denied, "A broadcast should no longer be refused for want of the option, not: " .. tostring(routeErr))

    -- A UDP socket joins and leaves a multicast group and refuses an address that is not one.
    assert(udp:joinGroup("239.255.42.99"):await() == "ok", "The method \"joinGroup\" should resolve")
    assert(udp:leaveGroup("239.255.42.99"):await() == "ok", "The method \"leaveGroup\" should resolve")
    local _, notGroup = udp:joinGroup("127.0.0.1"):await()
    assert(notGroup == 'The address "127.0.0.1" is not a multicast group.', "A unicast address should be refused as a group, not: " .. tostring(notGroup))
    udp:close():await()
    local _, closedGroup = udp:joinGroup("239.255.42.99"):await()
    assert(closedGroup == "The socket was closed.", "A closed socket should refuse to join, not: " .. tostring(closedGroup))

    print("The \"socket\" options tests passed.")
end)
