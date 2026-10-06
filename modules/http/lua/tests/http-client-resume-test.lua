-- A streamed download hands the head over before its body, stops at its limit with a message naming it, and resumes from where it stopped with a range.
local async = require("async")
local http = require("http")

local port
local base

local whole = {}
for index = 1, 1000 do
    whole[index] = string.char(97 + index % 26)
end
whole = table.concat(whole)

-- Reads a request header whatever the case its name arrived in.
local function header(ctx, wanted)
    for name, value in pairs(ctx.req.headers) do
        if name:lower() == wanted then
            return value
        end
    end
end

local app = http.createApp()

-- Serves the same bytes on every request, and only the tail of them for an open range, the way a download server answers a resume.
app:get("/file", function(ctx)
    local first = tonumber((header(ctx, "range") or ""):match("^bytes=(%d+)%-$"))
    if not first then
        ctx:header("X-File", "whole"):text(whole)
        return
    end

    ctx:status(206)
        :header("X-File", "part")
        :header("Content-Range", "bytes " .. first .. "-" .. (#whole - 1) .. "/" .. #whole)
        :text(whole:sub(first + 1))
end)

port = app:listen({ host = "127.0.0.1", port = 0 }).port
base = "http://127.0.0.1:" .. port

async.run(function()
    -- The head arrives before any piece of the body, with every header of the response.
    local order = {}
    local done = http.client.stream({
        url = base .. "/file",
        timeoutSeconds = 10,
        onResponse = function(status, headers)
            order[#order + 1] = "head"
            assert(status == 200, "Unexpected status " .. tostring(status))
            assert(headers["x-file"] == "whole", "The head must carry the headers of the response.")
        end,
    }, function()
        order[#order + 1] = "chunk"
    end):await()
    assert(done == "ok", "The stream did not complete.")
    assert(order[1] == "head" and order[2] == "chunk", "The head must arrive before the body.")

    -- A body past its limit ends the stream with a message that names the limit and the address.
    local collected = {}
    local cut, err = http.client.stream({ url = base .. "/file", timeoutSeconds = 10, maxResponseBytes = 400 }, function(chunk)
        collected[#collected + 1] = chunk
    end):await()
    assert(cut == nil, "A body past its limit must fail.")
    assert(tostring(err):find('"maxResponseBytes"', 1, true) and tostring(err):find("400", 1, true), "Unexpected failure: " .. tostring(err))
    assert(tostring(err):find(base .. "/file", 1, true), "The failure must name the address: " .. tostring(err))

    -- The download resumes from what it already holds, and the server answers the rest with a partial response.
    local held = table.concat(collected)
    assert(#held <= 400, "No more than the limit may arrive.")
    local partial
    local rest = {}
    done = http.client.stream({
        url = base .. "/file",
        timeoutSeconds = 10,
        headers = { Range = "bytes=" .. #held .. "-" },
        onResponse = function(status, headers)
            partial = { status = status, range = headers["content-range"] }
        end,
    }, function(chunk)
        rest[#rest + 1] = chunk
    end):await()
    assert(done == "ok", "The resumed stream did not complete.")
    assert(partial.status == 206, "A resume is answered with a partial response, got " .. tostring(partial.status))
    assert(partial.range == "bytes " .. #held .. "-999/1000", "Unexpected range " .. tostring(partial.range))
    assert(held .. table.concat(rest) == whole, "The resumed download must equal the whole one.")

    -- The buffered request reads the same range.
    local answer = http.client.requestRaw({ url = base .. "/file", headers = { Range = "bytes=990-" } }):await()
    assert(answer.status == 206 and answer.body == whole:sub(991), "The buffered request must read the range.")
    assert(answer.headers["content-range"] == "bytes 990-999/1000", "Unexpected range " .. tostring(answer.headers["content-range"]))

    print("The \"http\" client resume tests passed.")
end)
