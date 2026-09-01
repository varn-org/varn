-- an in-process server answers a route and a 404, exercised through the client
local async = require("async")
local http = require("http")

local port = 3991

local function statusBody(res)
    return res.status, res.body
end

http.createServer(function(req, res)
    if req.path == "/text" then
        res:finish("hello-http")
        return
    end
    res:status(404)
    res:finish("not found")
end):listen({ host = "127.0.0.1", port = port })

local function get(path)
    return http.client.requestRaw({
        url = "http://127.0.0.1:" .. port .. path,
        method = "GET",
        headers = {},
        timeoutSeconds = 10,
    }):await()
end

async.run(function()
    local wire, requestErr = get("/text")
    assert(not requestErr, requestErr)
    local status, body = statusBody(wire)
    assert(status == 200, "expected 200, got " .. status)
    assert(body == "hello-http", "unexpected body: " .. body)

    local missingWire, missingErr = get("/missing")
    assert(not missingErr, missingErr)
    assert(statusBody(missingWire) == 404, "expected 404")

    print("http ok")
end)
