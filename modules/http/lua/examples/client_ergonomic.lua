-- the ergonomic http client where get/post return a parsed response with status, ok, headers, body and json()
local async = require("async")

async.run(function()
    local http = require("http")

    -- a get with a query table appends a proper query string and parses the json response on demand
    local base = os.getenv("VARN_HTTP_URL") or "https://httpbin.org"
    local resp = http.client.get(base .. "/get", { query = { name = "varn", lang = "en" } }):await()
    print("get status", resp.status, "ok", resp.ok)
    print("echoed name", resp.json().args.name)

    -- a post with a json option serializes the body and sets a Content-Type of application/json
    local posted = http.client.post(base .. "/post", { json = { value = 42, tags = { "a", "b" } } }):await()
    print("post status", posted.status)
    print("server saw json", posted.json().json.value)
end)
