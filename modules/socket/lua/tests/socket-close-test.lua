-- Closing a socket or a listener rejects every operation still waiting on it with the close, while the end of a stream the peer closed stays an empty string.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"

async.run(function()
    -- A pending accept rejects with the close of its listener, and so does an accept after it.
    local listener = socket.tcp.listen(host, 0):await()
    local pendingAccept = listener:accept()
    async.sleep(5):await()
    assert(pendingAccept:isDone() == false, "An accept should wait while no connection arrives")
    listener:close():await()
    local accepted, acceptErr = pendingAccept:await()
    assert(accepted == nil and acceptErr == "The listener was closed.", "A pending accept should reject with the close, not: " .. tostring(acceptErr))
    local _, lateAcceptErr = listener:accept():await()
    assert(lateAcceptErr == "The listener was closed.", "An accept after the close should reject with it, not: " .. tostring(lateAcceptErr))

    -- A pending receive rejects with the close of its own socket.
    local server = socket.tcp.listen(host, 0):await()
    local conn = socket.tcp.connect(host, server.port):await()
    local peer = server:accept():await()
    local pendingReceive = conn:receive()
    async.sleep(5):await()
    assert(pendingReceive:isDone() == false, "A receive should wait while nothing arrives")
    conn:close():await()
    local data, receiveErr = pendingReceive:await()
    assert(data == nil and receiveErr == "The socket was closed.", "A pending receive should reject with the close, not: " .. tostring(receiveErr))

    -- The end of a stream the peer closed is still an empty string.
    local eof = peer:receive():await()
    assert(eof == "", "The peer should read the end of the stream as an empty string, not: " .. tostring(eof))
    peer:close():await()

    -- A send still waiting for a peer that does not read rejects with the close.
    -- The sends grow until one waits, since how much a system buffers on loopback differs between systems.
    local writer = socket.tcp.connect(host, server.port, { maxQueuedBytes = 1024 * 1024 * 1024 }):await()
    local idle = server:accept():await()
    local block = string.rep("x", 8 * 1024 * 1024)
    local pendingSend
    for _ = 1, 64 do
        pendingSend = writer:send(block)
        async.sleep(10):await()
        if not pendingSend:isDone() then
            break
        end
    end

    assert(pendingSend:isDone() == false, "A send to a peer that does not read should end up waiting")
    writer:close():await()
    local sent, sendErr = pendingSend:await()
    assert(sent == nil and sendErr == "The socket was closed.", "A pending send should reject with the close, not: " .. tostring(sendErr))
    assert(writer.pendingBytes == 0, "A closed socket should queue nothing")
    idle:close():await()
    server:close():await()

    -- A pending receive on a UDP socket rejects with the close, and so does a batch receive.
    local udp = socket.udp.bind(host, 0):await()
    local pendingFrom = udp:recvFrom()
    local pendingMany = udp:recvMany()
    async.sleep(5):await()
    udp:close():await()
    local datagram, fromErr = pendingFrom:await()
    assert(datagram == nil and fromErr == "The socket was closed.", "A pending recvFrom should reject with the close, not: " .. tostring(fromErr))
    local batch, manyErr = pendingMany:await()
    assert(batch == nil and manyErr == "The socket was closed.", "A pending recvMany should reject with the close, not: " .. tostring(manyErr))
    local _, lateSendErr = udp:sendTo(host, 9, "late"):await()
    assert(lateSendErr == "The socket was closed.", "A send after the close should reject with it, not: " .. tostring(lateSendErr))

    print("The \"socket\" close tests passed.")
end)
