-- http client ergonomics hitting an in-process server through the high-level request/get/post surface
local async = require("async")
local http = require("http")

local port = 39822
local base = "http://127.0.0.1:" .. port

local app = http.createApp()

-- echoes the parsed query so the test can prove the query option is appended correctly
app:get("/greet", function(ctx)
    ctx:json({ hello = ctx.query.name or "world", lang = ctx.query.lang })
end)

-- echoes a json request body so the test can prove the json option is serialized and typed
app:post("/echo", function(ctx)
    local body = ctx:body()
    ctx:json({ received = body.value, contentType = ctx.req.headers["Content-Type"] })
end)

app:listen({ host = "127.0.0.1", port = port })

async.run(function()
    -- get with a query table returns an ergonomic response whose json() parses the body
    local resp = http.client.get(base .. "/greet", { query = { name = "varn", lang = "en" } }):await()
    assert(resp.status == 200, "get status not 200")
    assert(resp.ok == true, "get ok flag wrong")
    assert(type(resp.headers) == "table", "headers is not a table")
    assert(resp.json().hello == "varn", "query value not echoed")
    assert(resp.json().lang == "en", "second query value not echoed")

    -- post with a json option serializes the body and sets the content type
    local posted = http.client.post(base .. "/echo", { json = { value = 42 } }):await()
    assert(posted.ok, "post not ok")
    local decoded = posted.json()
    assert(decoded.received == 42, "json body not round-tripped")
    assert(decoded.contentType == "application/json", "json content type not set")

    -- a not-found path resolves with ok = false but does not error
    local missing = http.client.get(base .. "/nope"):await()
    assert(missing.status == 404, "missing path status wrong")
    assert(missing.ok == false, "missing path ok flag wrong")

    -- the generic request() drives any method from one options table and parses like get/post
    local viaRequest = http.client.request({
        url = base .. "/echo",
        method = "POST",
        json = { value = 7 },
    }):await()
    assert(viaRequest.ok, "request() not ok")
    assert(viaRequest.json().received == 7, "request() json body not round-tripped")

    -- the raw primitive resolves to a status, headers, body table for low-level callers
    local raw = http.client.requestRaw({ url = base .. "/greet" }):await()
    assert(raw.status == 200, "raw status not 200")
    assert(type(raw.headers) == "table", "raw headers not a table")
    assert(raw.body:find("hello", 1, true), "raw body missing payload")

    print("http client ok")
end)
