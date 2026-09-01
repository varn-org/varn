-- an in-process unix-domain server echoes one message back to a client and both shut down
local async = require("async")
local socket = require("socket")

local path = os.tmpname() .. ".sock"
os.remove(path)

async.spawn(function()
    local listener = socket.unix.listen(path, 16):await()
    local client = listener:accept():await()
    local chunk = client:receive(4096):await()
    client:send("echo:" .. chunk):await()
    client:close():await()
    listener:close():await()
end)

async.run(function()
    async.sleep(80):await()

    local conn = socket.unix.connect(path):await()
    conn:send("hello"):await()
    local reply = conn:receive(4096):await()
    print("unix reply:", reply)
    conn:close():await()
    os.remove(path)

    print("socket unix round-trip ok")
end)
