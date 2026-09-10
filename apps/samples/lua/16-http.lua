-- The http client needs the network, so this one fails without a connection.
local async = require("async")
local http = require("http")

async.run(function()
    print("encoded:", http.urlEncode("a b&c=d"))
    print("decoded:", http.urlDecode("a%20b%26c"))

    local response, err = http.client.get("https://example.com"):await()
    if err then
        print("request failed:", err)
        return
    end

    print("status:", response.status)
    print("bytes:", #response.body)
end)
