-- Names resolve off the loop thread for connects, listeners and datagrams, `socket.resolve` lists every address of a host, and a connected UDP socket sends with `send`.
local async = require("async")
local socket = require("socket")

local function contains(list, value)
    for _, item in ipairs(list) do
        if item == value then
            return true
        end
    end

    return false
end

async.run(function()
    -- A name lists its addresses, a numeric address answers itself and a name that does not exist rejects with it.
    local loopback = socket.resolve("localhost"):await()
    assert(type(loopback) == "table" and #loopback >= 1, "The name \"localhost\" should resolve to at least one address")
    assert(contains(loopback, "127.0.0.1") or contains(loopback, "::1"), "The name \"localhost\" should resolve to a loopback address")
    local numeric = socket.resolve("127.0.0.1"):await()
    assert(#numeric == 1 and numeric[1] == "127.0.0.1", "A numeric address should answer itself")
    local missing, missingErr = socket.resolve("host.invalid"):await()
    assert(missing == nil and missingErr == 'The host "host.invalid" could not be resolved.', "An unknown host should reject with its name, not: " .. tostring(missingErr))
    assert(not pcall(socket.resolve), "A resolve without a host should be refused")

    -- A connect to a name reaches a listener bound to the same name once the name resolved.
    local listener = socket.tcp.listen("localhost", 0):await()
    local conn, connErr = socket.tcp.connect("localhost", listener.port):await()
    assert(conn and not connErr, "A connect to a name should succeed, not: " .. tostring(connErr))
    local accepted = listener:accept():await()
    conn:send("named"):await()
    assert(accepted:receive():await() == "named", "The connection to a name should carry data")
    conn:close():await()
    accepted:close():await()
    listener:close():await()

    -- A connect with a deadline still reports a name that does not exist.
    local unknown, unknownErr = socket.tcp.connect("host.invalid", 80, { timeoutMs = 2000 }):await()
    assert(unknown == nil and unknownErr == 'The host "host.invalid" could not be resolved.', "An unknown host should reject with its name, not: " .. tostring(unknownErr))

    -- A connected UDP socket sends with `send` and the peer answers with `sendTo`, both bound to the same name.
    local server = socket.udp.bind("localhost", 0):await()
    local client = socket.udp.bind("localhost", 0):await()
    local _, notConnected = client:send("early"):await()
    assert(notConnected == 'The socket is not connected. Connect it with "connect" before calling "send".', "A send before connect should be refused, not: " .. tostring(notConnected))

    assert(client:connect("localhost", server.port):await() == "ok", "A UDP connect to a name should succeed")
    assert(client:send("hello"):await() == "ok", "A connected UDP socket should send")
    local packet = server:recvFrom():await()
    assert(packet.data == "hello" and packet.port == client.port, "The server should receive the datagram from the client")
    server:sendTo(packet.host, packet.port, "reply"):await()
    local reply = client:recvFrom():await()
    assert(reply.data == "reply" and reply.port == server.port, "The connected client should receive the reply of its peer")

    -- A datagram addressed by a name leaves once the name resolved.
    server:sendTo("localhost", client.port, "by name"):await()
    assert(client:recvFrom():await().data == "by name", "A datagram addressed by a name should arrive")
    local _, sendToErr = server:sendTo("host.invalid", client.port, "lost"):await()
    assert(sendToErr == 'The host "host.invalid" could not be resolved.', "A datagram to an unknown host should reject with its name, not: " .. tostring(sendToErr))

    -- The batched calls keep working, with names in the batch resolved before it leaves.
    assert(server:sendMany({ { host = "localhost", port = client.port, data = "one" }, { host = client.host, port = client.port, data = "two" } }):await() == 2, "A batch with a name should send every datagram")
    local received = {}
    while #received < 2 do
        for _, datagram in ipairs(client:recvMany():await()) do
            received[#received + 1] = datagram.data
        end
    end

    assert(received[1] == "one" and received[2] == "two", "The batch should arrive in order")

    server:close():await()
    client:close():await()

    print("The \"socket\" resolve tests passed.")
end)
