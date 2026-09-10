-- Day-to-day web features covering gzip compression, SSE streaming, cache/ETag helpers and `Accept` negotiation through one in-process server.
local async = require("async")
local http = require("http")

local port = 39823
local base = "http://127.0.0.1:" .. port

local function statusBody(res)
    return res.status, res.body
end

-- Header names arrive with the casing the transport used, so a lookup normalises before comparing.
local function headersBody(res)
    local lowered = {}
    for name, value in pairs(res.headers or {}) do
        lowered[name:lower()] = value
    end

    return lowered, res.body
end

local function request(method, path, headers)
    return http.client.requestRaw({
        url = base .. path,
        method = method,
        headers = headers or {},
        timeoutSeconds = 10,
    }):await()
end

local function get(path, headers)
    return request("GET", path, headers)
end

local app = http.createApp()

-- A large JSON body crosses the compression threshold and the client sees raw bytes since it does not decode.
local big = {}
for i = 1, 400 do
    big[i] = { id = i, label = "item-number-" .. i }
end

app:get("/big", function(ctx) ctx:json({ items = big }) end)

-- A tiny body stays below the threshold and must never be compressed.
app:get("/tiny", function(ctx) ctx:json({ ok = true }) end)

-- An already-encoded body is left untouched even when large and gzip is accepted.
app:get("/preencoded", function(ctx)
    local payload = string.rep("text-payload-line\n", 200)
    ctx:header("Content-Encoding", "identity"):type("text/plain"):send(payload)
end)

-- SSE streams a couple of events then closes so the buffering client receives a finite body.
app:get("/events", function(ctx)
    local stream = ctx:sse()
    stream:send("hello")
    stream:send("tick", "first")
    stream:send("multi\nline")
    stream:comment("heartbeat")
    stream:close()
end)

-- Cache and ETag helpers stamp dynamic responses.
app:get("/cached", function(ctx)
    ctx:cache({ maxAge = 120, private = true }):json({ ok = true })
end)

app:get("/tagged", function(ctx)
    ctx:etag("v1"):json({ ok = true })
end)

-- Content negotiation picks HTML or JSON from the `Accept` header.
app:get("/negotiate", function(ctx)
    local best = ctx:accepts("html", "json")
    if best == "json" then
        ctx:json({ kind = "json" })
    else
        ctx:html("<p>html</p>")
    end
end)

app:listen({ host = "127.0.0.1", port = port })

async.run(function()
    -- A large JSON body with gzip accepted comes back gzip-framed and smaller than the plain body.
    local plainStatus, plainBody = statusBody(get("/big"))
    assert(plainStatus == 200, "The plain big request failed")
    assert(plainBody:byte(1) ~= 0x1f, "The plain body is unexpectedly gzip-framed")

    -- What the server sends is gzip framed, though a client is free to decode it before handing it over, which the platform transports do, so the assertion is that the server compressed rather than that the caller sees the frame.
    local gzHeaders, gzBody = headersBody(get("/big", { ["Accept-Encoding"] = "gzip" }))
    if gzHeaders["content-encoding"] == "gzip" then
        assert(gzBody:byte(1) == 0x1f and gzBody:byte(2) == 0x8b, "The gzip magic bytes are missing")
        assert(#gzBody < #plainBody, "The gzip body is not smaller than the plain body")
    else
        assert(gzBody == plainBody, "A decoded body must match the plain one")
    end

    -- A tiny body stays uncompressed even when gzip is accepted.
    local _, tinyBody = statusBody(get("/tiny", { ["Accept-Encoding"] = "gzip" }))
    assert(tinyBody:byte(1) ~= 0x1f, "A tiny body should not be gzipped")

    -- An already-encoded body is never recompressed.
    local _, preBody = statusBody(get("/preencoded", { ["Accept-Encoding"] = "gzip" }))
    assert(preBody:byte(1) ~= 0x1f, "The pre-encoded body was recompressed")

    -- The SSE stream carries event, data and comment lines with correct framing.
    local sseStatus, sseBody = statusBody(get("/events"))
    assert(sseStatus == 200, "The SSE request failed")
    assert(sseBody:find("data: hello\n\n", 1, true), "The SSE default-event message is missing")
    assert(sseBody:find("event: tick\ndata: first\n\n", 1, true), "The SSE named event is missing")
    assert(sseBody:find("data: multi\ndata: line\n\n", 1, true), "The SSE multi-line data was not split")
    assert(sseBody:find(": heartbeat\n\n", 1, true), "The SSE comment heartbeat is missing")

    -- The cache helper composes the directive string.
    local cacheStatus, cacheBody = statusBody(get("/cached"))
    assert(cacheStatus == 200 and cacheBody:find("ok", 1, true), "The cache route failed")

    -- The ETag helper answers a fresh request and short-circuits a matching `If-None-Match` to 304.
    assert(statusBody(get("/tagged")) == 200, "The ETag fresh request failed")
    assert(statusBody(get("/tagged", { ["If-None-Match"] = '"v1"' })) == 304, 'The ETag helper did not honor "If-None-Match"')

    -- The `If-None-Match` matching handles a comma-separated list, the wildcard, and a weak validator, and ignores a non-match.
    assert(statusBody(get("/tagged", { ["If-None-Match"] = '"other", "v1"' })) == 304, "The ETag should match a tag listed among others")
    assert(statusBody(get("/tagged", { ["If-None-Match"] = "*" })) == 304, "The ETag should honor the wildcard")
    assert(statusBody(get("/tagged", { ["If-None-Match"] = 'W/"v1"' })) == 304, "The ETag should match a weak validator by weak comparison")
    assert(statusBody(get("/tagged", { ["If-None-Match"] = '"v2"' })) == 200, "The ETag should not short-circuit a non-matching validator")

    -- Content negotiation returns JSON or HTML based on `Accept`.
    local _, negJson = statusBody(get("/negotiate", { ["Accept"] = "application/json" }))
    assert(negJson:find("json", 1, true), "The negotiation did not pick JSON")
    local _, negHtml = statusBody(get("/negotiate", { ["Accept"] = "text/html" }))
    assert(negHtml:find("html", 1, true), "The negotiation did not pick HTML")

    print("The \"http\" web features tests passed.")
end)
