-- static file serving where a full request carries an ETag and Accept-Ranges, a Range request returns 206 with a Content-Range and the partial body, and a matching If-None-Match returns 304
local async = require("async")
local http = require("http")
local socket = require("socket")
local fs = require("fs")

local host = "127.0.0.1"
local port = 39830
local dir = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set")

local app = http.createApp()

local function send(request)
    local conn = socket.tcp.connect(host, port):await()
    conn:send(request):await()
    local buffer = ""
    while true do
        local chunk, err = conn:receive(4096):await()
        if not chunk or chunk == "" or err then
            break
        end
        buffer = buffer .. chunk
    end
    conn:close():await()
    return buffer
end

local function request(headers)
    local lines = { "GET /asset.txt HTTP/1.1", "Host: " .. host }
    for _, header in ipairs(headers) do
        lines[#lines + 1] = header
    end
    lines[#lines + 1] = "Connection: close"
    lines[#lines + 1] = ""
    lines[#lines + 1] = ""
    return send(table.concat(lines, "\r\n"))
end

local function headerValue(response, pattern)
    return response:match("\r\n" .. pattern .. ": ([^\r\n]+)")
end

local function bodyOf(response)
    local headerEnd = response:find("\r\n\r\n", 1, true)
    assert(headerEnd, "response has no header terminator")
    return response:sub(headerEnd + 4)
end

async.run(function()
    fs.writeFile(dir .. "/asset.txt", "0123456789"):await()
    app:listen({ host = host, port = port, publicDir = dir, servePublic = true })

    -- a full request serves 200 with an ETag and the Accept-Ranges advertisement
    local full = request({})
    assert(full:find(" 200 ", 1, true), "a full request should be 200")
    assert(bodyOf(full) == "0123456789", "the full body should be served")
    local etag = headerValue(full, "ETag")
    assert(etag, "a static file should carry an ETag")
    assert(headerValue(full, "Accept%-Ranges") == "bytes", "the server should advertise byte ranges")

    -- a range request serves 206 with a Content-Range and only the requested bytes
    local partial = request({ "Range: bytes=0-4" })
    assert(partial:find(" 206 ", 1, true), "a range request should be 206")
    assert(headerValue(partial, "Content%-Range") == "bytes 0-4/10", "the content range should be reported")
    assert(bodyOf(partial) == "01234", "the partial body should be the requested range")

    -- a matching If-None-Match yields 304 with no body
    local cached = request({ "If-None-Match: " .. etag })
    assert(cached:find(" 304 ", 1, true), "a matching etag should be 304")
    assert(bodyOf(cached) == "", "a 304 response should have no body")

    print("http static range ok")
end)
