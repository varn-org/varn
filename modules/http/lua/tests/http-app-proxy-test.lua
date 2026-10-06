-- Behind a trusted proxy the rate limiter keys a client by the address that proxy appended, the rightmost entry of "X-Forwarded-For", so a client that forges the entries before it still meets its limit.
local async = require("async")
local http = require("http")

local host = "127.0.0.1"
local port

local app = http.createApp()
app:use(http.rateLimit({ windowMs = 60000, max = 2, trustProxy = true }))
app:get("/ping", function(ctx)
    ctx:text("pong")
end)
port = app:listen({ host = host, port = 0 }).port

local function hit(forwarded)
    local response = http.client.get("http://" .. host .. ":" .. port .. "/ping", {
        headers = { ["X-Forwarded-For"] = forwarded },
    }):await()
    return response.status
end

async.run(function()
    -- Every request names another forged client first, while the proxy appended the same real address.
    assert(hit("203.0.113.1, 10.0.0.7") == 200, "The first request within the limit should pass.")
    assert(hit("203.0.113.2, 10.0.0.7") == 200, "The second request within the limit should pass.")
    assert(hit("203.0.113.3, 10.0.0.7") == 429, "A forged entry before the address of the proxy must not escape the limit.")

    -- Another real client the proxy saw keeps its own quota.
    assert(hit("203.0.113.3, 10.0.0.8") == 200, "Another client behind the proxy should keep its own limit.")

    print("The \"http\" rate limit proxy tests passed.")
end)
