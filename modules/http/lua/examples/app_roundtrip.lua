-- drives an in-process app with the client to show routing, body parsing and a json response then exits, with the server on its own threads so the script issues its requests inside async.run and finishes cleanly
local async = require("async")
local http = require("http")

local port = 8091
local base = "http://127.0.0.1:" .. port

local app = http.createApp()

-- a path parameter constrained to digits feeds a json response
app:get("/users/:id", function(ctx)
    ctx:json({ id = ctx.params.id })
end):where("id", "int")

-- json body parsing detects the content type and hands back a parsed table
app:post("/echo", function(ctx)
    ctx:json({ received = ctx:body() })
end)

app:listen({ host = "127.0.0.1", port = port })

async.run(function()
    local user = http.client.requestRaw({
        url = base .. "/users/7",
        method = "GET",
        headers = {},
        timeoutSeconds = 10,
    }):await()
    print("GET /users/7 -> " .. user.status .. " " .. user.body)

    local payload = '{"name":"varn"}'
    local echo = http.client.requestRaw({
        url = base .. "/echo",
        method = "POST",
        headers = { ["Content-Type"] = "application/json" },
        body = payload,
        timeoutSeconds = 10,
    }):await()
    print("POST /echo -> " .. echo.status .. " " .. echo.body)
end)
