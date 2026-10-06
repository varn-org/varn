-- The client keeps one deadline for the whole request, takes a timeout with a fraction and refuses one that is no positive number.
local async = require("async")
local http = require("http")

local port
local base

local app = http.createApp()

-- Answers at once, so a short timeout with a fraction is enough for it.
app:get("/quick", function(ctx)
    ctx:text("quick")
end)

-- Trickles one event every fifth of a second for five seconds, so every wait between two pieces stays short while the whole answer takes long.
app:get("/trickle", function(ctx)
    local stream = ctx:sse()

    for index = 1, 25 do
        stream:send("piece " .. index)
        async.sleep(200):await()
    end

    stream:close()
end)

port = app:listen({ host = "127.0.0.1", port = 0 }).port
base = "http://127.0.0.1:" .. port

async.run(function()
    local answer = http.client.request({ url = base .. "/quick", timeoutSeconds = 2.5 }):await()
    assert(answer.status == 200 and answer.body == "quick", "A timeout with a fraction must be accepted.")

    -- A server that keeps sending still has to finish within the one deadline of the request.
    local started = os.time()
    local trickled, err = http.client.request({ url = base .. "/trickle", timeoutSeconds = 1 }):await()
    assert(trickled == nil, "A request past its deadline must fail.")
    assert(tostring(err):find("did not finish within its timeout", 1, true), "Unexpected failure: " .. tostring(err))
    assert(os.time() - started <= 3, "The deadline must end the request long before the server stops sending.")

    -- The same deadline holds for a streamed request, whose pieces arrive until it passes.
    local pieces = 0
    started = os.time()
    local streamed
    streamed, err = http.client.stream({ url = base .. "/trickle", timeoutSeconds = 1 }, function()
        pieces = pieces + 1
    end):await()
    assert(streamed == nil, "A stream past its deadline must fail.")
    assert(tostring(err):find("did not finish within its timeout", 1, true), "Unexpected failure: " .. tostring(err))
    assert(pieces > 0, "The pieces sent before the deadline must arrive.")
    assert(os.time() - started <= 3, "The deadline must end the stream long before the server stops sending.")

    for _, wrong in ipairs({ 0, -1, "10", math.huge, 0 / 0, true }) do
        local refused
        refused, err = http.client.request({ url = base .. "/quick", timeoutSeconds = wrong }):await()
        assert(refused == nil, "The timeout " .. tostring(wrong) .. " must be refused.")
        assert(tostring(err):find('"timeoutSeconds"', 1, true), "Unexpected refusal: " .. tostring(err))
    end

    print("The \"http\" client deadline tests passed.")
end)
