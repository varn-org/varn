-- A server that cannot take its address fails the call with the cause, rather than with an exception Lua cannot read.
local async = require("async")
local http = require("http")

async.run(function()
    local app = http.createApp()
    local ok, err = pcall(app.listen, app, { host = "256.0.0.1", port = 8799 })
    assert(not ok, "An app listening on an address nobody can hold must fail.")
    assert(tostring(err):find("could not listen", 1, true), "The failure of an app names what failed: " .. tostring(err))

    local server = http.createServer(function() end)
    ok, err = pcall(server.listen, server, { host = "256.0.0.1", port = 8799 })
    assert(not ok, "A server listening on an address nobody can hold must fail.")
    assert(tostring(err):find("could not listen", 1, true), "The failure of a server names what failed: " .. tostring(err))

    print("The \"http.listen\" tests passed.")
end)
