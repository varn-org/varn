-- shows hello echo and hashed file routes in one process
local http = require("http")
local async = require("async")
local fs = require("fs")
local crypto = require("crypto")

local server = http.createServer(function(req, res)
    if req.path == "/api/hello" then
        async.sleep(10):await()

        local name = req.query.name or "World"
        res:json({
            message = "Hello " .. name,
            host = req.host,
            method = req.method,
            remoteAddress = req.remoteAddress
        })
        return
    end

    if req.path == "/api/echo" then
        res:json({
            method = req.method,
            body = req.body,
            contentType = req.headers["Content-Type"] or req.headers["content-type"] or ""
        })
        return
    end

    if req.path == "/api/file" then
        local content, err = fs.readFile("apps/lua/public/index.html"):await()
        if err then
            res:status(500)
            res:finish(err)
            return
        end

        res:json({
            size = #content,
            sha256 = crypto.digest("SHA256", content, "hex")
        })
        return
    end

    res:status(404)
    res:finish("not found")
end)

server:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    publicDir = "apps/lua/public",
    servePublic = true
})
