-- serves json over tls using files in the working directory or varn tls env variables
local http = require("http")

local server = http.createServer(function(req, res)
    res:json({
        ok = true,
        scheme = "https",
        host = req.host,
        path = req.path
    })
end)

server:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3443"),
    tls = true,
    certFile = os.getenv("VARN_TLS_CERT") or "cert.pem",
    keyFile = os.getenv("VARN_TLS_KEY") or "key.pem",
    publicDir = "apps/lua/public",
    servePublic = true
})
