-- Batched UDP sends and receives, and a listener that serves several waiting accepts at once.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"

async.run(function()
    local server = socket.udp.bind(host, 0):await()
    local client = socket.udp.bind(host, 0):await()
    local serverPort = server.port
    local clientPort = client.port

    -- A batch of datagrams leaves in order and resolves to how many were sent.
    local batch = {}
    for i = 1, 100 do
        batch[i] = { host = host, port = serverPort, data = "datagram-" .. i }
    end

    assert(client:sendMany(batch):await() == 100, "The method \"sendMany\" should resolve to the count it sent.")

    -- The method `recvMany` waits for the first datagram, then takes every datagram already waiting up to its limit.
    local received = {}
    while #received < 100 do
        local datagrams, err = server:recvMany(30):await()
        assert(not err, err)
        assert(#datagrams >= 1 and #datagrams <= 30, "A batch should hold between one datagram and the limit, not " .. #datagrams .. ".")
        for _, datagram in ipairs(datagrams) do
            assert(datagram.host == host and datagram.port == clientPort, "Each datagram should carry the address of its sender.")
            received[#received + 1] = datagram.data
        end
    end

    for i = 1, 100 do
        assert(received[i] == "datagram-" .. i, "The datagrams should arrive in the order they were sent.")
    end

    -- A pending batch receive settles once a datagram arrives later.
    local pending = server:recvMany()
    async.sleep(5):await()
    assert(pending:isDone() == false, "A batch receive should wait while nothing arrives.")
    client:sendTo(host, serverPort, "late"):await()
    local late = pending:await()
    assert(#late == 1 and late[1].data == "late", "A batch receive should settle with the datagram that arrived.")

    -- An empty batch sends nothing and resolves at once.
    assert(client:sendMany({}):await() == 0, "An empty batch should resolve to zero.")

    -- A batch with a malformed entry is refused before anything is sent.
    local ok, message = pcall(client.sendMany, client, { { host = host, port = serverPort, data = "fine" }, { host = host, port = 0, data = "bad" } })
    assert(not ok and message:find("index 2", 1, true), "A bad port should be refused by its index, not: " .. tostring(message))
    ok, message = pcall(client.sendMany, client, { { port = serverPort, data = "no host" } })
    assert(not ok and message:find("\"host\"", 1, true), "A missing host should be refused, not: " .. tostring(message))
    ok, message = pcall(server.recvMany, server, 0)
    assert(not ok, "A batch limit of zero should be refused.")

    -- A batch to an address that cannot be resolved rejects with the index of the datagram.
    local _, unresolved = client:sendMany({ { host = "host.invalid", port = serverPort, data = "x" } }):await()
    assert(unresolved and unresolved:find("index 1", 1, true), "An unresolvable address should reject by its index, not: " .. tostring(unresolved))

    -- A receive pending when its socket closes settles instead of hanging.
    local orphan = server:recvMany()
    server:close():await()
    local closedData, closedErr = orphan:await()
    assert(closedData == nil and closedErr == "The socket was closed.", "A batch receive should reject once its socket closes, not: " .. tostring(closedErr))
    client:close():await()

    -- Several accepts that wait at once each get their own connection, and none fails for lack of one.
    local listener = socket.tcp.listen(host, 0, { backlog = 64 }):await()
    local accepts = {}
    for i = 1, 5 do
        accepts[i] = listener:accept()
    end

    local connections = {}
    for i = 1, 5 do
        connections[i] = socket.tcp.connect(host, listener.port)
    end

    for i = 1, 5 do
        local accepted, err = accepts[i]:await()
        assert(accepted and not err, "Every waiting accept should get a connection, not: " .. tostring(err))
        accepted:close():await()
        connections[i]:await():close():await()
    end

    listener:close():await()

    print("The \"socket\" batch tests passed.")
end)
