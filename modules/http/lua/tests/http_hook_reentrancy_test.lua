-- a hook or event handler that registers another one of its own kind while it runs must not corrupt the list being walked
local async = require("async")
local http = require("http")

local host = "127.0.0.1"
local port = 39835

local app = http.createApp()

local requestHookRuns = 0
local responseHookRuns = 0
local startHookRuns = 0
local eventRuns = 0

-- the first hook of each kind grows its own list mid-dispatch and a second plain hook follows it, so the dispatcher must still read the entry after the one that reallocated
app:onStart(function()
    startHookRuns = startHookRuns + 1
    for _ = 1, 16 do
        app:onStart(function()
            startHookRuns = startHookRuns + 1
        end)
    end
end)

app:onStart(function()
    startHookRuns = startHookRuns + 1
end)

app:onRequest(function()
    requestHookRuns = requestHookRuns + 1
    if requestHookRuns == 1 then
        for _ = 1, 16 do
            app:onRequest(function()
                requestHookRuns = requestHookRuns + 1
            end)
        end
    end
end)

app:onRequest(function()
    requestHookRuns = requestHookRuns + 1
end)

app:onResponse(function()
    responseHookRuns = responseHookRuns + 1
    if responseHookRuns == 1 then
        for _ = 1, 16 do
            app:onResponse(function()
                responseHookRuns = responseHookRuns + 1
            end)
        end
    end
end)

app:onResponse(function()
    responseHookRuns = responseHookRuns + 1
end)

app:on("tick", function()
    eventRuns = eventRuns + 1
    if eventRuns == 1 then
        for _ = 1, 16 do
            app:on("tick", function()
                eventRuns = eventRuns + 1
            end)
        end
    end
end)

app:on("tick", function()
    eventRuns = eventRuns + 1
end)

app:get("/ping", function(ctx)
    ctx:text("pong")
end)

app:listen({ host = host, port = port })

async.run(function()
    assert(startHookRuns == 2, "only the two hooks registered before listen run at start, got " .. startHookRuns)

    local first = http.client.get("http://" .. host .. ":" .. port .. "/ping"):await()
    assert(first.status == 200, "the first request should succeed")
    assert(requestHookRuns == 2, "only the two originally registered onRequest hooks run on the first request, got " .. requestHookRuns)
    assert(responseHookRuns == 2, "only the two originally registered onResponse hooks run on the first request, got " .. responseHookRuns)

    -- the second request walks the grown lists, which is where a stale iterator would show up
    local second = http.client.get("http://" .. host .. ":" .. port .. "/ping"):await()
    assert(second.status == 200, "the second request should succeed after the hook lists grew")
    assert(requestHookRuns == 20, "every onRequest hook should run on the second request, got " .. requestHookRuns)
    assert(responseHookRuns == 20, "every onResponse hook should run on the second request, got " .. responseHookRuns)

    app:emit("tick")
    assert(eventRuns == 2, "only the two originally registered handlers run on the first emit, got " .. eventRuns)

    app:emit("tick")
    assert(eventRuns == 20, "every handler should run on the second emit, got " .. eventRuns)

    print("http hook reentrancy ok")
end)
