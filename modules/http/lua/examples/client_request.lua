local async = require("async")

async.spawn(function()
    local http = require("http")
    local url = os.getenv("VARN_HTTP_URL") or "https://httpbin.org/get"
    local response, err = http.client.requestRaw({
        url = url,
        method = "GET",
        headers = {},
        timeoutSeconds = 30
    }):await()
    if err then
        error(err)
    end
    print("status", response.status)
    print("body", response.body)
end)
