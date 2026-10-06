-- A server binds a port of its own unless it asks to share it, a busy port fails with a clear message, port 0 takes a free port, and the options of `listen` refuse a bad value or an unknown name by name.
local async = require("async")
local http = require("http")

local host = "127.0.0.1"

local function refused(options, expected)
    local server = http.createServer(function() end)
    local ok, err = pcall(server.listen, server, options)
    assert(not ok, "The options must be refused: " .. tostring(expected))
    assert(tostring(err):find(expected, 1, true), "The refusal names what is wrong, expected " .. expected .. " in: " .. tostring(err))
end

local function get(port)
    return http.client.requestRaw({ url = "http://" .. host .. ":" .. port .. "/", timeoutSeconds = 10 }):await()
end

async.run(function()
    -- Port 0 takes a free port, and two such servers take two different ports.
    local first = http.createServer(function(_, res) res:finish("first") end):listen({ host = host, port = 0 })
    local second = http.createServer(function(_, res) res:finish("second") end):listen({ host = host, port = 0 })
    assert(first.port > 0 and second.port > 0 and first.port ~= second.port, "Port 0 takes a free port for each server")
    assert(get(first.port).body == "first" and get(second.port).body == "second", "Each server answers on its own port")

    -- A second server on a busy port fails with a message that names the address, and the first one keeps its connections.
    local busy = http.createServer(function(_, res) res:finish("thief") end)
    local ok, err = pcall(busy.listen, busy, { host = host, port = first.port })
    assert(not ok, "A second server on a busy port must fail")
    local expected = "The address " .. host .. ":" .. first.port .. " is already in use."
    assert(tostring(err):find(expected, 1, true), "The failure names the busy address, got: " .. tostring(err))
    local app = http.createApp()
    ok, err = pcall(app.listen, app, { host = host, port = first.port })
    assert(not ok and tostring(err):find("already in use", 1, true), "An app on a busy port fails the same way, got: " .. tostring(err))
    for _ = 1, 4 do
        assert(get(first.port).body == "first", "The first server keeps every connection of its port")
    end

    -- Two servers that both ask to share a port may bind it.
    local shared = http.createServer(function(_, res) res:finish("shared") end):listen({ host = host, port = 0, reusePort = true })
    local sharing = http.createServer(function(_, res) res:finish("shared") end):listen({ host = host, port = shared.port, reusePort = true })
    assert(sharing.port == shared.port, "A server that shares the port binds it too")
    assert(get(shared.port).body == "shared", "A shared port answers")
    shared:close():await()
    sharing:close():await()

    -- A port is an integer from 0 to 65535 in both forms of `listen`.
    refused({ host = host, port = 70000 }, 'The option "port" must be an integer from 0 to 65535.')
    refused({ host = host, port = -1 }, 'The option "port" must be an integer from 0 to 65535.')
    refused({ host = host, port = "80" }, 'The option "port" must be an integer from 0 to 65535.')
    refused({ host = host, port = 1.5 }, 'The option "port" must be an integer from 0 to 65535.')
    refused(70000, 'The option "port" must be an integer from 0 to 65535.')
    refused("3000", 'The method "listen" takes a port or a table of options.')

    -- An unknown option is refused by name instead of being ignored.
    refused({ host = host, port = 0, prot = 8080 }, 'The listen option "prot" is unknown.')
    refused({ host = host, port = 0, maxThreads = 4 }, 'The listen option "maxThreads" is unknown.')

    -- A value of the wrong type or out of range is refused by the name of its option.
    refused({ host = 42, port = 0 }, 'The option "host" must be a string.')
    refused({ host = host, port = 0, compress = "yes" }, 'The option "compress" must be a boolean.')
    refused({ host = host, port = 0, maxRequestBodyBytes = 0 }, 'The option "maxRequestBodyBytes" must be an integer from 1 to')
    refused({ host = host, port = 0, maxQueued = 0 }, 'The option "maxQueued" must be an integer from 1 to 65535.')
    refused({ host = host, port = 0, requestTimeoutMs = -5 }, 'The option "requestTimeoutMs" must be an integer from 0 to')

    -- Static files need a folder to serve.
    refused({ host = host, port = 0, servePublic = true }, 'The option "servePublic" needs the option "publicDir".')

    first:close():await()
    second:close():await()

    print("The \"http\" listen options tests passed.")
end)
