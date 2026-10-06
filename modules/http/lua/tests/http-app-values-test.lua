-- The context refuses a value it cannot honor with a sentence that names it instead of sending another one, keeps the status a handler set for a file, and takes a media type the client weighs at zero as refused.
local async = require("async")
local http = require("http")
local fs = require("fs")

local host = "127.0.0.1"
local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set.')

-- Runs the call and answers the text of its error, or "accepted" when it raised none.
local function refusal(call)
    local ok, err = pcall(call)
    return ok and "accepted" or tostring(err)
end

local app = http.createApp()

app:get("/missing", function(ctx)
    ctx:status(404):file(dir .. "/missing.html")
end)

app:get("/refusals", function(ctx)
    local found = {
        status = refusal(function() ctx:status(42) end),
        redirect = refusal(function() ctx:redirect("/", 1000) end),
        json = refusal(function() ctx:json("text") end),
        cache = refusal(function() ctx:cache({ maxAge = 1.5 }) end),
        framing = refusal(function() ctx:header("Transfer-Encoding", "chunked") end),
        connection = refusal(function() ctx:header("connection", "close") end),
    }
    ctx:json(found)
end)

app:get("/accepts", function(ctx)
    ctx:text(ctx:accepts("json", "html") or "none")
end)

local server = app:listen({ host = host, port = 0 })

local function fetch(path, headers)
    return http.client.get("http://" .. host .. ":" .. server.port .. path, { headers = headers }):await()
end

local function expect(found, key, text)
    assert(found[key]:find(text, 1, true), "The refusal of \"" .. key .. "\" should say \"" .. text .. "\", not \"" .. found[key] .. "\".")
end

async.run(function()
    fs.writeFile(dir .. "/missing.html", "<p>missing</p>"):await()

    -- A file keeps the status the handler set before it.
    local missing = fetch("/missing")
    assert(missing.status == 404, "A file sent behind a status should keep it, not " .. missing.status .. ".")
    assert(missing.body == "<p>missing</p>", "The file should still be sent.")

    -- Each value the context cannot honor raises a sentence that names it.
    local found = fetch("/refusals").json()
    expect(found, "status", "The status 42 is not an HTTP status, which runs from 100 to 599.")
    expect(found, "redirect", "The status 1000 is not an HTTP status")
    expect(found, "json", 'The method "json" takes a table.')
    expect(found, "cache", 'The option "maxAge" must be an integer.')
    expect(found, "framing", 'The header "Transfer-Encoding" belongs to the server')
    expect(found, "connection", 'The header "connection" belongs to the server')

    -- A route whose wildcard is not its last segment is refused when it is added.
    local wildcard = refusal(function() app:get("/files/*/raw", function() end) end)
    assert(wildcard:find('The wildcard "*" of the route "/files/*/raw" must be its last segment.', 1, true), "An inner wildcard should be refused, not " .. wildcard .. ".")

    -- A type the client weighs at zero is refused, even when a wildcard follows it.
    assert(fetch("/accepts", { Accept = "application/json;q=0, */*" }).body == "html", "A type weighed at zero should be refused.")
    assert(fetch("/accepts", { Accept = "text/html;q=0, application/json" }).body == "json", "The type the client accepts should be chosen.")
    assert(fetch("/accepts", { Accept = "*/*;q=0" }).body == "none", "A wildcard weighed at zero should accept nothing.")

    server:close():await()
    print("The \"http\" context value tests passed.")
end)
