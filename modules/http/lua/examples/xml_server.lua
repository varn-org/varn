-- serves small xml payloads plus static files from the public tree
local http = require("http")

local server = http.createServer(function(req, res)
    local name = req.query.name or "Nobody"

    res:xml({
        ok = "true",
        scheme = "http",
        host = req.host,
        path = req.path,
        name = name
    })
end)

server:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    servePublic = true
})
