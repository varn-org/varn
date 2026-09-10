local async = require("async")
local http = require("http")

local PORT = 8791
local BASE = "http://127.0.0.1:" .. PORT

--- Answers a server that says every way a redirect can be said, so each rule is exercised against one.
local function serve()
    local app = http.createApp()

    app:get("/end", function(ctx) ctx:text("arrived") end)
    app:post("/end", function(ctx) ctx:text("posted " .. tostring(ctx:body())) end)

    app:get("/hop/:left", function(ctx)
        local left = tonumber(ctx.params.left)

        if left <= 0 then
            return ctx:redirect("/end", 302)
        end

        ctx:redirect("/hop/" .. (left - 1), 302)
    end)

    -- A location beside the one that was asked for rather than one from the root.
    app:get("/beside/start", function(ctx) ctx:redirect("landed", 302) end)
    app:get("/beside/landed", function(ctx) ctx:text("beside") end)

    app:get("/root", function(ctx) ctx:redirect("/end", 302) end)

    -- The two that turn what was asked into a plain read, and the two that do not.
    app:post("/becomes-get", function(ctx) ctx:redirect("/end", 303) end)
    app:post("/keeps-post", function(ctx) ctx:redirect("/end", 307) end)

    -- A redirect that says nothing about where to go is the answer itself.
    app:get("/nowhere", function(ctx) ctx:status(302):text("") end)

    -- One that never stops.
    app:get("/forever", function(ctx) ctx:redirect("/forever", 302) end)

    app:listen({ host = "127.0.0.1", port = PORT })
end

--- Answers the response and whatever went wrong, which is how the client reports a failure.
local function get(path, options)
    options = options or {}
    options.url = BASE .. path
    options.timeoutSeconds = 10
    return http.client.requestRaw(options):await()
end

local function refused(path, options)
    local answer, problem = get(path, options)
    return answer == nil, tostring(problem)
end

async.run(function()
    serve()
    async.sleep(120):await()

    -- Following is what happens unless the caller says otherwise, which is what every client does.
    do
        local answer = get("/root")
        assert(answer.status == 200, "a redirect is followed by default, got " .. answer.status)
        assert(answer.body == "arrived", "the answer is the one at the end of the chain")
    end

    -- A caller who wants to see the redirect asks for it and is handed it untouched.
    do
        local answer = get("/root", { redirect = "manual" })
        assert(answer.status == 302, "a caller asking to see the redirect must see it")
    end

    -- One who wants a redirect to be a failure says so.
    do
        local failed, problem = refused("/root", { redirect = "error" })
        assert(failed, "a redirect refused must fail rather than be followed")
        assert(problem:find("redirect", 1, true) ~= nil, "and say what failed: " .. problem)
    end

    -- A chain is followed to its end, and one longer than allowed fails rather than going for ever.
    do
        local answer = get("/hop/5")
        assert(answer.status == 200, "a chain of redirects is followed to its end")

        assert(refused("/hop/5", { maxRedirects = 2 }), "a chain longer than it is allowed to be must fail")
        assert(refused("/root", { maxRedirects = 0 }), "being allowed no redirects at all means exactly that")
    end

    -- A location standing beside what was asked for is resolved against it.
    do
        local answer = get("/beside/start")
        assert(answer.status == 200 and answer.body == "beside",
            "a relative location resolves against the request it answered, got " .. tostring(answer.body))
    end

    -- What a redirect does to the method is what the status says it does.
    do
        local turned = get("/becomes-get", { method = "POST", body = "sent" })
        assert(turned.body == "arrived", "a 303 turns what was asked into a plain read")

        local kept = get("/keeps-post", { method = "POST", body = "sent" })
        assert(kept.body == "posted sent", "a 307 keeps the method and the body, got " .. tostring(kept.body))
    end

    -- A redirect with nowhere to go is the answer itself rather than a failure.
    do
        local answer = get("/nowhere")
        assert(answer.status == 302, "a redirect naming nowhere is handed over as it is")
    end

    -- One that never ends is refused rather than followed for ever.
    do
        assert(refused("/forever"), "a redirect that never ends must be given up on")
    end

    -- A name nobody meant is refused where it was written rather than taken as something else.
    do
        local ok = pcall(function() return get("/root", { redirect = "sometimes" }) end)
        assert(not ok, "a policy nobody offers must be refused")
    end

    print("http.redirects ok")
end)
