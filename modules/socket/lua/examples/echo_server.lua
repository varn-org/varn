-- echo service with an overridable listen port through the environment

local async = require("async")
local socket = require("socket")

local host = os.getenv("VARN_SOCKET_HOST") or "127.0.0.1"
local port = tonumber(os.getenv("VARN_SOCKET_PORT") or "9000")

local function handleClient(sock)
    while true do
        local chunk, err = sock:receive(4096):await()
        if err then
            print("client receive error:", err)
            break
        end
        if #chunk == 0 then
            break
        end
        local _, sendErr = sock:send("You sent: " .. chunk):await()
        if sendErr then
            print("client send error:", sendErr)
            break
        end
    end
    sock:close():await()
end

async.spawn(function()
    local listener, lerr = socket.tcp.listen(host, port, 128):await()
    if lerr then
        error(lerr)
    end
    print(string.format("tcp echo listening on %s:%d", host, port))
    while true do
        local client, aerr = listener:accept():await()
        if aerr then
            print("accept error:", aerr)
            break
        end
        async.spawn(function()
            handleClient(client)
        end)
    end
    listener:close():await()
end)
