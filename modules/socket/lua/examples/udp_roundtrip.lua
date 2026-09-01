-- an in-process udp server echoes one datagram back and both sockets close
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local serverPort = 9841
local clientPort = 9842

async.spawn(function()
    local server = socket.udp.bind(host, serverPort):await()
    local packet = server:recvFrom(4096):await()
    server:sendTo(packet.host, packet.port, "echo:" .. packet.data):await()
    server:close():await()
end)

async.run(function()
    async.sleep(80):await()

    local client = socket.udp.bind(host, clientPort):await()
    client:sendTo(host, serverPort, "hello"):await()
    local reply = client:recvFrom(4096):await()
    print("udp reply:", reply.data, "from", reply.host, reply.port)
    client:close():await()

    print("socket udp round-trip ok")
end)
