-- creates an HTTP server and binds it to a local port using explicit transport options
local http = require("http")

local port = 8080

http.createServer(function(_, res)
    res:finish("ok")
end):listen({ host = "127.0.0.1", port = port })

print("http server listening on http://127.0.0.1:" .. port)
