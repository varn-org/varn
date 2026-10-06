-- Listeners and UDP sockets bind port 0 to a free port they report, refuse a busy port by its address, and share one only when every listener asks for it.
local async = require("async")
local platform = require("platform")
local socket = require("socket")

local host = "127.0.0.1"
local windows = platform.os() == "windows"

async.run(function()
    -- Port 0 binds a free port, which the listener reports with its host.
    local listener = socket.tcp.listen(host, 0):await()
    assert(listener.host == host, "The listener should report its host, not: " .. tostring(listener.host))
    assert(math.type(listener.port) == "integer" and listener.port > 0, "The listener should report the port it bound, not: " .. tostring(listener.port))

    local conn = socket.tcp.connect(host, listener.port):await()
    local accepted = listener:accept():await()

    -- The side of the listener closes first, so its port keeps a connection in TIME_WAIT.
    accepted:close():await()
    conn:close():await()

    -- A second listener on a busy port fails with the address instead of sharing it.
    local second, busy = socket.tcp.listen(host, listener.port):await()
    assert(second == nil, "A second listener on a busy port should not bind")
    assert(busy == "The address " .. host .. ":" .. listener.port .. " is already in use.", "A busy port should be reported by its address, not: " .. tostring(busy))

    -- A port the closed listener left behind binds again, even with its connection still in TIME_WAIT.
    local port = listener.port
    listener:close():await()
    local again, againErr = socket.tcp.listen(host, port):await()
    assert(again and not againErr, "A closed listener's port should bind again, not: " .. tostring(againErr))
    again:close():await()

    -- Listeners share a port only when each of them asks for it, which Windows does not offer.
    local first, firstErr = socket.tcp.listen(host, 0, { reusePort = true }):await()
    if windows then
        assert(first == nil and firstErr == 'The option "reusePort" is not available on Windows.', "The option \"reusePort\" should be refused on Windows, not: " .. tostring(firstErr))
    else
        assert(first and not firstErr, firstErr)
        local shared, sharedErr = socket.tcp.listen(host, first.port, { reusePort = true }):await()
        assert(shared and not sharedErr, "A listener asking for a shared port should bind, not: " .. tostring(sharedErr))
        assert(shared.port == first.port, "Both listeners should hold the same port")
        local alone = socket.tcp.listen(host, first.port):await()
        assert(alone == nil, "A listener that does not ask for a shared port should not bind it")
        shared:close():await()
        first:close():await()
    end

    -- An IPv6 listener reports its host and bound port the same way.
    local six = socket.tcp.listen("::1", 0):await()
    if six then
        assert(six.host == "::1" and six.port > 0, "The IPv6 listener should report its address")
        six:close():await()
    end

    -- The options of a listener are checked before anything is bound.
    assert(not pcall(socket.tcp.listen, host, 0, 16), "A backlog outside an options table should be refused")
    assert(not pcall(socket.tcp.listen, host, 0, { backlog = 0 }), "A backlog of zero should be refused")
    assert(not pcall(socket.tcp.listen, host, 0, { reusePort = "yes" }), "A reusePort that is not a boolean should be refused")
    assert(not pcall(socket.tcp.listen, host, 0, { maxQueuedBytes = 0 }), "A maxQueuedBytes of zero should be refused")

    -- A UDP socket binds port 0 the same way and refuses a busy port by its address.
    local udp = socket.udp.bind(host, 0):await()
    assert(udp.host == host and math.type(udp.port) == "integer" and udp.port > 0, "The UDP socket should report its address")
    local other, otherErr = socket.udp.bind(host, udp.port):await()
    assert(other == nil, "A second UDP socket on a busy port should not bind")
    assert(otherErr == "The address " .. host .. ":" .. udp.port .. " is already in use.", "A busy UDP port should be reported by its address, not: " .. tostring(otherErr))
    udp:close():await()

    -- A listener on a host name resolves it off the loop thread and reports the address it bound.
    local named = socket.tcp.listen("localhost", 0):await()
    assert(named.host == "127.0.0.1" or named.host == "::1", "A listener on a name should report its address, not: " .. tostring(named.host))
    named:close():await()

    print("The \"socket\" bind tests passed.")
end)
