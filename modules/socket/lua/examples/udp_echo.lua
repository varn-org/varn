-- udp echo service with an overridable bind address through the environment

local async = require("async")
local socket = require("socket")

local host = os.getenv("VARN_SOCKET_HOST") or "127.0.0.1"
local port = tonumber(os.getenv("VARN_SOCKET_PORT") or "9000")

async.spawn(function()
    local sock, berr = socket.udp.bind(host, port):await()
    if berr then
        error(berr)
    end
    print(string.format("udp echo listening on %s:%d", host, port))
    while true do
        local packet, rerr = sock:recvFrom(4096):await()
        if rerr then
            print("recv error:", rerr)
            break
        end
        local _, serr = sock:sendTo(packet.host, packet.port, "You sent: " .. packet.data):await()
        if serr then
            print("send error:", serr)
            break
        end
    end
    sock:close():await()
end)
