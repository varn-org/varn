-- caching and content negotiation covering cache-control, etag revalidation, and api-vs-html responses from one route
local http = require("http")

local app = http.createApp()

-- a long-lived cached resource sets cache-control plus an etag the client can revalidate against
app:get("/profile/:id", function(ctx)
    local id = ctx.params.id
    ctx:cache({ maxAge = 300, private = true })
    ctx:etag("profile-" .. id .. "-v3")

    -- if etag matched the request's If-None-Match, the helper already answered 304 and ended the response
    if ctx.req.headers["If-None-Match"] then
        return
    end

    ctx:json({ id = id, name = "User " .. id })
end)

-- the same path serves html to a browser and json to an api client based on Accept
app:get("/report", function(ctx)
    local best = ctx:accepts("html", "json")
    if best == "json" then
        ctx:cache(60):json({ title = "Quarterly report", revenue = 1000 })
    else
        ctx:cache(60):html("<h1>Quarterly report</h1><p>Revenue: 1000</p>")
    end
end)

-- a never-cache endpoint for volatile data
app:get("/now", function(ctx)
    ctx:cache({ noStore = true }):json({ time = os.time() })
end)

app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
})
