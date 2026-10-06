-- A datagram larger than a receive takes fails that receive with its size on every system, the sends of a UDP socket are bounded by its "maxQueuedBytes", and a limit out of range is refused by its name.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"

async.run(function()
    local server = socket.udp.bind(host, 0):await()
    local client = socket.udp.bind(host, 0, { maxQueuedBytes = 1000 }):await()
    local from = host .. ":" .. client.port
    local on = host .. ":" .. server.port

    -- The datagram is taken whole and refused by its size, and the next one arrives as it was sent.
    client:sendTo(host, server.port, string.rep("x", 600)):await()
    local packet, err = server:recvFrom(100):await()
    assert(packet == nil, "A datagram larger than the receive takes must fail the receive.")
    assert(err == "The datagram of 600 bytes from " .. from .. " is larger than the 100 bytes the receive on " .. on .. " takes.", tostring(err))
    client:sendTo(host, server.port, "small"):await()
    assert(server:recvFrom(100):await().data == "small", "The datagram after a refused one must arrive whole.")

    -- A batch delivers the datagrams taken before one that is too large, and the next receive answers that one.
    client:sendTo(host, server.port, "one"):await()
    client:sendTo(host, server.port, string.rep("y", 300)):await()
    async.sleep(50):await()
    local batch = server:recvMany(8, 100):await()
    assert(#batch == 1 and batch[1].data == "one", "The datagram before the large one must be delivered.")
    local none, deferred = server:recvMany(8, 100):await()
    assert(none == nil and tostring(deferred):find("The datagram of 300 bytes from " .. from, 1, true), tostring(deferred))

    -- A send that would queue past the bound is refused while the queued one still leaves, and the room comes back once it left.
    local first = client:sendTo(host, server.port, string.rep("a", 800))
    local _, refused = client:sendTo(host, server.port, string.rep("b", 300)):await()
    assert(refused == "The socket already queues 800 bytes, so a send of 300 bytes would pass its limit of 1000 bytes.", tostring(refused))
    assert(first:await() == "ok", "The queued send must still leave.")
    assert(client:sendTo(host, server.port, string.rep("c", 300)):await() == "ok", "A send must fit once the queue drained.")

    local ok, message = pcall(socket.udp.bind, host, 0, { maxQueuedBytes = 0 })
    assert(not ok and tostring(message):find('The option "maxQueuedBytes" must be an integer from 1 to ', 1, true), tostring(message))
    ok, message = pcall(server.recvFrom, server, 1 << 40)
    assert(not ok and tostring(message):find("The maximum number of bytes to receive must be an integer from 1 to 2147483647.", 1, true), tostring(message))

    client:close():await()
    server:close():await()

    print("The \"socket\" UDP limits tests passed.")
end)
