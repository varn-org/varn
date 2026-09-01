-- server-sent events plus gzip with a live clock stream and a large json endpoint the server compresses automatically
local http = require("http")
local async = require("async")

local app = http.createApp()

-- the /clock route streams sse events a browser reads through EventSource
app:get("/clock", function(ctx)
    local stream = ctx:sse()
    for i = 1, 10 do
        stream:send("tick", os.date("%H:%M:%S"))
        stream:comment("keep-alive")
        async.sleep(1000):await()
        local _ = i
    end
    stream:close()
end)

-- a large json body is gzipped when the client sends an Accept-Encoding of gzip
app:get("/data", function(ctx)
    local rows = {}
    for i = 1, 500 do
        rows[i] = { id = i, name = "row-" .. i, note = "a reasonably long descriptive label" }
    end
    ctx:json({ rows = rows })
end)

app:get("/", function(ctx)
    ctx:html([[
<!doctype html>
<h1>SSE clock</h1>
<pre id="out"></pre>
<script>
const out = document.getElementById("out");
const es = new EventSource("/clock");
es.addEventListener("tick", e => out.textContent += e.data + "\n");
</script>
]])
end)

-- gzip is on by default and compress = false on listen turns it off
app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
    compress = true,
})
