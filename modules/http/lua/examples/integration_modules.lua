-- combines async file reads hashing and static responses on one host from the repo root
local http = require("http")
local async = require("async")
local fs = require("fs")
local crypto = require("crypto")

local dataDir = "build/_integration_tmp"

local function route(req, res)
    if req.path == "/write" then
        fs.writeFile(dataDir .. "/hello.txt", "Created by Varn."):await()
        res:finish("written")
        return
    end

    if req.path == "/read" then
        local value, err = fs.readFile(dataDir .. "/hello.txt"):await()
        if err then
            res:status(404)
            res:finish(err)
            return
        end
        res:finish(value)
        return
    end

    if req.path == "/hash" then
        res:finish(crypto.digest("SHA256", req.query.value or "varn", "hex"))
        return
    end

    if req.path == "/sleep" then
        async.sleep(3000):await()
        res:finish("done")
        return
    end

    res:finish("Varn modules example.")
end

http.createServer(route):listen(tonumber(os.getenv("VARN_PORT") or "3000"))
