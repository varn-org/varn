-- The client refuses every option of the wrong type or out of range by its name, and a download that cannot write its file settles instead of waiting forever.
local async = require("async")
local http = require("http")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')
local nowhere = "http://127.0.0.1:1/"

-- Calls a raw primitive of the client with the options and asserts that it raises the refusal it should.
local function refuses(call, options, wanted)
    local ok, err = pcall(call, options, function() end)
    assert(not ok, "The options must be refused with: " .. wanted)
    assert(tostring(err):find(wanted, 1, true), "Expected the refusal " .. wanted .. " but got " .. tostring(err))
end

local requestRaw = http.client.requestRaw
refuses(requestRaw, {}, 'The option "url" must be the address of the request.')
refuses(requestRaw, { url = 80 }, 'The option "url" must be the address of the request.')
refuses(requestRaw, { url = nowhere, method = 5 }, 'The option "method" must be a string.')
refuses(requestRaw, { url = nowhere, headers = "Accept: */*" }, 'The option "headers" must be a table.')
refuses(requestRaw, { url = nowhere, headers = { "Accept" } }, 'Every name of the option "headers" must be a string.')
refuses(requestRaw, { url = nowhere, headers = { Accept = true } }, 'The header "Accept" of the option "headers" must be a string or a number.')
refuses(requestRaw, { url = nowhere, body = {} }, 'The option "body" must be a string.')
refuses(requestRaw, { url = nowhere, insecure = 1 }, 'The option "insecure" must be a boolean.')
refuses(requestRaw, { url = nowhere, verifyTls = false }, 'The request option "verifyTls" is unknown.')
refuses(requestRaw, { url = nowhere, path = dir .. "/never" }, 'The request option "path" is unknown.')
refuses(requestRaw, { url = nowhere, [1] = "x" }, "An option must be named by a string.")
refuses(http.client.stream, { url = nowhere, timeout = 5 }, 'The request option "timeout" is unknown.')
refuses(http.client.downloadRaw, { url = nowhere, path = dir .. "/never", onChunk = print }, 'The request option "onChunk" is unknown.')
refuses(requestRaw, { url = nowhere, maxResponseBytes = 1.5 }, 'The option "maxResponseBytes" must be an integer from 1 to ')
refuses(requestRaw, { url = nowhere, maxResponseBytes = 0 }, 'The option "maxResponseBytes" must be an integer from 1 to ')
refuses(requestRaw, { url = nowhere, redirect = true }, 'The option "redirect" must be "follow", "manual" or "error".')
refuses(requestRaw, { url = nowhere, maxRedirects = 2 ^ 40 }, 'The option "maxRedirects" must be an integer from 0 to 2147483647.')
refuses(requestRaw, { url = nowhere, maxRedirects = -1 }, 'The option "maxRedirects" must be an integer from 0 to 2147483647.')
refuses(http.client.downloadRaw, { url = nowhere, path = dir .. "/never", resume = "yes" }, 'The option "resume" must be a boolean.')
refuses(http.client.downloadRaw, { url = nowhere, path = dir .. "/never", onProgress = 5 }, 'The option "onProgress" must be a function.')
refuses(http.client.stream, { url = nowhere, onResponse = 5 }, 'The option "onResponse" must be a function.')
refuses(http.client.stream, { url = nowhere, query = "a=1" }, 'The option "query" must be a table.')

-- A number is a header value too, written the way Lua writes it.
local app = http.createApp()
app:get("/echo", function(ctx)
    ctx:text(tostring(ctx.request.headers["X-Count"]))
end)
app:get("/file", function(ctx)
    ctx:header("ETag", '"v1"'):text("contents")
end)

local base = "http://127.0.0.1:" .. app:listen({ host = "127.0.0.1", port = 0 }).port

async.run(function()
    local answer = http.client.get(base .. "/echo", { headers = { ["X-Count"] = 42 } }):await()
    assert(answer.body == "42", "A number must be sent as a header value, got " .. tostring(answer.body))

    -- The ergonomic calls hand an option they do not know to the request, which refuses it by name.
    local _, unknown = http.client.get(base .. "/echo", { timeout = 5 }):await()
    assert(tostring(unknown):find('The request option "timeout" is unknown.', 1, true), "An unknown option must be refused, got " .. tostring(unknown))

    -- The partial file cannot be opened under a name this long, and the download that resumes must still settle with that failure.
    local path = dir .. "/" .. string.rep("n", 300)
    local download = http.client.download({ url = base .. "/file", path = path, resume = true, timeoutSeconds = 10 })
    local result, err = async.timeout(download, 5000):await()
    assert(result == nil, "A download whose file cannot be opened must fail.")
    assert(tostring(err):find("could not open the file", 1, true), "Unexpected failure: " .. tostring(err))

    print("The \"http\" client options tests passed.")
end)
