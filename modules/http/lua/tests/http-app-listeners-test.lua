-- One application that listens on two servers serves the static files of each server from the folder that server named, so the second listen never changes what the first serves.
local async = require("async")
local http = require("http")
local fs = require("fs")

local host = "127.0.0.1"
local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set.')

local app = http.createApp()
app:get("/route", function(ctx)
    ctx:text("route")
end)

local function fetch(port, path)
    return http.client.get("http://" .. host .. ":" .. port .. path):await()
end

async.run(function()
    fs.mkdir(dir .. "/first"):await()
    fs.mkdir(dir .. "/second"):await()
    fs.writeFile(dir .. "/first/who.txt", "first"):await()
    fs.writeFile(dir .. "/second/who.txt", "second"):await()

    local first = app:listen({ host = host, port = 0, publicDir = dir .. "/first" })
    local second = app:listen({ host = host, port = 0, publicDir = dir .. "/second" })
    local plain = app:listen({ host = host, port = 0 })

    assert(fetch(first.port, "/who.txt").body == "first", "The first server should serve its own folder.")
    assert(fetch(second.port, "/who.txt").body == "second", "The second server should serve its own folder.")
    assert(fetch(plain.port, "/who.txt").status == 404, "A server without a folder should serve no files.")
    assert(fetch(plain.port, "/route").body == "route", "Every server should serve the routes of the application.")

    first:close():await()
    second:close():await()
    plain:close():await()
    print("The \"http\" application listener tests passed.")
end)
