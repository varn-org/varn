-- the rate limiter bounds its own per-client store, so a flood of distinct addresses cannot grow it without end and the limit still applies afterwards
local async = require("async")
local http = require("http")

local host = "127.0.0.1"
local port = 39836

local app = http.createApp()
app:use(http.rateLimit({ windowMs = 60000, max = 3, trustProxy = true, maxClients = 8 }))
app:get("/ping", function(ctx)
    ctx:text("pong")
end)
app:listen({ host = host, port = port })

local function hit(ip)
    local response = http.client.get("http://" .. host .. ":" .. port .. "/ping", {
        headers = { ["X-Forwarded-For"] = ip },
    }):await()
    return response.status
end

async.run(function()
    -- a single client is allowed its quota and then throttled
    for i = 1, 3 do
        assert(hit("10.0.0.1") == 200, "request " .. i .. " within the limit should pass")
    end
    assert(hit("10.0.0.1") == 429, "the request past the limit should be throttled")

    -- far more distinct addresses than the store may hold arrive inside the same window
    for i = 1, 20 do
        assert(hit("10.1.0." .. i) == 200, "the first request from a fresh address should pass")
    end

    -- the store was reset rather than grown, so the throttled client is tracked afresh
    assert(hit("10.0.0.1") == 200, "the store should have been bounded, clearing the earlier bucket")

    -- and the limit still applies to that client after the reset
    for i = 2, 3 do
        assert(hit("10.0.0.1") == 200, "request " .. i .. " of the new window should pass")
    end
    assert(hit("10.0.0.1") == 429, "the limiter should still throttle after the store was bounded")

    print("http ratelimit bound ok")
end)
