-- A download writes its body to a file as it arrives and moves the file in place only once it is whole, removes it on a failure or a cancellation, resumes a partial file with a range, and a response read one chunk at a time and an upload from a file never hold a body either.
local async = require("async")
local fs = require("fs")
local http = require("http")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')

local piece = {}
for index = 1, 64 * 1024 do
    piece[index] = string.char(97 + index % 26)
end
piece = table.concat(piece)

-- The body is 32 MiB, far more than the window of a stream, so a download that gathered it in memory would show in the Lua heap.
local pieces = 512
local size = #piece * pieces
local tag = '"v1"'
local served = { ranges = {}, uploads = {} }

-- Reads a request header whatever the case its name arrived in.
local function header(ctx, wanted)
    for name, value in pairs(ctx.req.headers) do
        if name:lower() == wanted then
            return value
        end
    end
end

-- Writes the bytes from `first` to the end of the body, piece by piece, awaiting each write so the server never runs ahead of the client.
local function writeFrom(ctx, first)
    local index = first // #piece
    local offset = first % #piece
    if offset > 0 then
        ctx:write(piece:sub(offset + 1)):await()
        index = index + 1
    end
    for _ = index + 1, pieces do
        local ok = ctx:write(piece):await()
        if not ok then
            return
        end
    end
end

local app = http.createApp()

-- Serves the body with a strong validator and honours an open range whose `If-Range` still matches it, the way a download server answers a resume.
-- A body whose length the server declares is refused before it arrives when it passes the limit, so the route of the failures halfway sends its body chunked.
local function serve(ctx, declared)
    local range = header(ctx, "range")
    local first = tonumber((range or ""):match("^bytes=(%d+)%-$"))
    served.ranges[#served.ranges + 1] = range or "none"
    ctx:header("ETag", tag)

    if first and header(ctx, "if-range") == tag then
        if first >= size then
            ctx:status(416):header("Content-Range", "bytes */" .. size):send("")
            return
        end

        ctx:status(206):header("Content-Range", "bytes " .. first .. "-" .. (size - 1) .. "/" .. size)
        if declared then
            ctx:header("Content-Length", tostring(size - first))
        end
        writeFrom(ctx, first)
        return
    end

    if declared then
        ctx:header("Content-Length", tostring(size))
    end
    writeFrom(ctx, 0)
end

app:get("/file", function(ctx)
    serve(ctx, true)
end)

app:get("/chunked", function(ctx)
    serve(ctx, false)
end)

-- Sends the first piece of its body and holds the rest until the test releases it, so a read of the rest is sure to wait.
local releaseHeld
app:get("/held", function(ctx)
    local held, release = async.deferred()
    releaseHeld = release
    ctx:write("first"):await()
    held:await()
    ctx:write("rest"):await()
end)

app:get("/missing", function(ctx)
    ctx:status(404):send("Nothing here.")
end)

app:post("/upload", function(ctx)
    served.uploads[#served.uploads + 1] = ctx.req.body
    ctx:text(tostring(#ctx.req.body))
end)

local base = "http://127.0.0.1:" .. app:listen({ host = "127.0.0.1", port = 0 }).port
local limit = 64 * 1024 * 1024

local function expected()
    return string.rep(piece, pieces)
end

-- Answers `length` bytes of the body from the offset `first` on, without building the whole body for each chunk a read hands over.
local function slice(first, length)
    local parts, offset, remaining = {}, first % #piece, length
    while remaining > 0 do
        local part = piece:sub(offset + 1, offset + remaining)
        parts[#parts + 1] = part
        remaining = remaining - #part
        offset = 0
    end
    return table.concat(parts)
end

local function read(path)
    return fs.readFile(path):await()
end

async.run(function()
    -- The body reaches the file as it arrives, with the progress of the download, and the Lua heap never holds it.
    collectgarbage("collect")
    local heap = collectgarbage("count")
    local target = dir .. "/downloads/whole.bin"
    local reports = 0
    local last = 0
    local result = http.client.download({
        url = base .. "/file",
        path = target,
        timeoutSeconds = 60,
        maxResponseBytes = limit,
        onProgress = function(received, total)
            reports = reports + 1
            assert(received >= last, "The progress must only grow.")
            assert(total == size, "The progress must report the total, got " .. tostring(total))
            last = received
        end,
    }):await()
    assert(collectgarbage("count") - heap < 4 * 1024, "The download must not gather its body in the Lua heap.")
    assert(result.status == 200 and result.size == size and result.path == target and result.resumed == false, "Unexpected result.")
    assert(result.headers.etag == tag, "The result must carry the headers of the response.")
    assert(reports > 0 and last == size, "The progress must reach the whole body, got " .. last)
    assert(not fs.exists(target .. ".part"), "The partial file must be gone once the file is whole.")
    assert(read(target) == expected(), "The file must hold the whole body.")

    -- A status that is no success writes no file and names the status.
    local missing = dir .. "/downloads/missing.bin"
    local none, refused = http.client.download({ url = base .. "/missing", path = missing }):await()
    assert(none == nil and tostring(refused):find("with the status 404.", 1, true), "Unexpected failure: " .. tostring(refused))
    assert(not fs.exists(missing) and not fs.exists(missing .. ".part"), "A refused download must leave no file.")

    -- A download that fails halfway leaves no file, and an existing file stays as it was.
    local kept = dir .. "/downloads/kept.bin"
    fs.writeFile(kept, "older"):await()
    local cut, failure = http.client.download({ url = base .. "/chunked", path = kept, timeoutSeconds = 60, maxResponseBytes = 8 * 1024 * 1024 }):await()
    assert(cut == nil and tostring(failure):find('"maxResponseBytes"', 1, true), "Unexpected failure: " .. tostring(failure))
    assert(not fs.exists(kept .. ".part"), "A failed download must remove its partial file.")
    assert(read(kept) == "older", "A failed download must leave the file it would replace alone.")

    -- A cancellation through a controller leaves no file either, and a controller that was cancelled cancels a request at once.
    local controller = http.client.controller()
    local cancelledPath = dir .. "/downloads/cancelled.bin"
    local stopped, reason = http.client.download({
        url = base .. "/file",
        path = cancelledPath,
        timeoutSeconds = 60,
        maxResponseBytes = limit,
        controller = controller,
        onProgress = function(received)
            if received > 4 * 1024 * 1024 then
                controller:cancel()
            end
        end,
    }):await()
    assert(stopped == nil and tostring(reason) == "The request was cancelled.", "Unexpected failure: " .. tostring(reason))
    assert(controller.cancelled, "The controller must report that it was cancelled.")
    assert(not fs.exists(cancelledPath) and not fs.exists(cancelledPath .. ".part"), "A cancelled download must leave no file.")
    local late, lateReason = http.client.requestRaw({ url = base .. "/missing", controller = controller }):await()
    assert(late == nil and lateReason == "The request was cancelled.", "A cancelled controller must cancel a new request at once.")

    -- A download that resumes keeps what it held when it fails and asks for the rest with a range and the validator of the first answer.
    local resumed = dir .. "/downloads/resumed.bin"
    local first = http.client.download({ url = base .. "/chunked", path = resumed, resume = true, timeoutSeconds = 60, maxResponseBytes = 8 * 1024 * 1024 }):await()
    assert(first == nil, "The first attempt must fail at its limit.")
    local held = fs.stat(resumed .. ".part"):await().size
    assert(held > 0 and held <= 8 * 1024 * 1024, "The partial file must stay for a resume, got " .. tostring(held))
    local rest = http.client.download({ url = base .. "/file", path = resumed, resume = true, timeoutSeconds = 60, maxResponseBytes = limit }):await()
    assert(rest.status == 206 and rest.resumed and rest.size == size, "The second attempt must continue the partial file.")
    assert(served.ranges[#served.ranges] == "bytes=" .. held .. "-", "The resume must ask for the rest, got " .. served.ranges[#served.ranges])
    assert(read(resumed) == expected(), "The resumed file must equal the whole body.")
    assert(not fs.exists(resumed .. ".part") and not fs.exists(resumed .. ".part-validator"), "A finished resume must remove what it kept.")

    -- A partial file that is already whole is answered with 416 and becomes the file without another byte.
    local complete = dir .. "/downloads/complete.bin"
    fs.writeFile(complete .. ".part", expected()):await()
    fs.writeFile(complete .. ".part-validator", tag):await()
    local done = http.client.download({ url = base .. "/file", path = complete, resume = true }):await()
    assert(done.status == 416 and done.resumed and done.size == size, "A whole partial file must become the file.")
    assert(read(complete) == expected(), "The whole partial file must be the file.")

    -- A partial file of another version of the body starts over, since its validator no longer matches.
    local changed = dir .. "/downloads/changed.bin"
    fs.writeFile(changed .. ".part", "stale bytes"):await()
    fs.writeFile(changed .. ".part-validator", '"v0"'):await()
    local again = http.client.download({ url = base .. "/file", path = changed, resume = true, timeoutSeconds = 60, maxResponseBytes = limit }):await()
    assert(again.status == 200 and not again.resumed, "A changed body must be downloaded whole.")
    assert(read(changed) == expected(), "A download that starts over must hold the new body alone.")

    -- A response read one chunk at a time hands each chunk to its read, ends with nil and holds only what its reads did not take.
    local response = http.client.open({ url = base .. "/file", timeoutSeconds = 60, maxResponseBytes = limit }):await()
    assert(response.status == 200 and response.ok and response.contentLength == size and response.headers.etag == tag, "Unexpected response.")
    local count, chunks = 0, 0
    local checked = true
    while true do
        local chunk = response:read():await()
        if chunk == nil then
            break
        end
        checked = checked and chunk == slice(count, #chunk)
        count = count + #chunk
        chunks = chunks + 1
    end
    assert(count == size and checked and chunks > 1, "The reads must return the whole body in order.")
    assert(response:read():await() == nil, "A read after the end must answer nil.")
    local busy = http.client.open({ url = base .. "/held", timeoutSeconds = 60 }):await()
    local opening = ""
    while opening ~= "first" do
        opening = opening .. busy:read():await()
    end
    local waiting = busy:read()
    local twice = pcall(busy.read, busy)
    assert(not twice, "A second read while one waits must fail.")
    releaseHeld()
    assert(waiting:await() == "rest", "The read that waited must take the rest of the body.")
    busy:cancel()
    local _, stoppedRead = busy:read():await()
    assert(stoppedRead == nil or stoppedRead == "The request was cancelled.", "Unexpected failure: " .. tostring(stoppedRead))

    -- An upload streams its body from a file.
    local source = dir .. "/upload.bin"
    fs.writeFile(source, piece .. piece):await()
    local uploaded = http.client.request({ url = base .. "/upload", method = "POST", bodyFile = source }):await()
    assert(uploaded.body == tostring(#piece * 2) and served.uploads[1] == piece .. piece, "The upload must send the whole file.")
    local both = pcall(http.client.requestRaw, { url = base .. "/upload", method = "POST", body = "x", bodyFile = source })
    assert(not both, "A request must refuse both a body and a body file.")

    -- A controller closed as a to-be-closed variable cancels its requests when its task is cancelled.
    local closing = dir .. "/downloads/closing.bin"
    local outcome
    local task = async.spawn(function()
        local scoped <close> = http.client.controller()
        outcome = http.client.download({ url = base .. "/file", path = closing, timeoutSeconds = 60, maxResponseBytes = limit, controller = scoped })
        outcome:await()
    end)
    async.sleep(20):await()
    task.cancel()
    local _, closed = outcome:await()
    assert(closed == "The request was cancelled.", "Unexpected failure: " .. tostring(closed))
    assert(not fs.exists(closing) and not fs.exists(closing .. ".part"), "A download its task ended must leave no file.")

    print("The \"http\" client download tests passed.")
end)
