-- websocket chat with rooms where each client joins a room and messages fan out to that room's members
local http = require("http")

local app = http.createApp()

-- a connection joins the room named in its query string, defaulting to "lobby"
app:ws("/chat", {
    open = function(conn)
        conn:join("lobby")
        conn:send("welcome to lobby")
    end,
    message = function(conn, data)
        -- a "/join <room>" command moves the sender while anything else broadcasts to the lobby
        local room = data:match("^/join%s+(%S+)$")
        if room then
            conn:leave("lobby")
            conn:join(room)
            conn:send("joined " .. room)
            return
        end

        app:wsBroadcastRoom("lobby", data)
    end,
    close = function()
        print("a chat client disconnected")
    end,
})

-- a notifications endpoint pushes to everyone on the path with app:wsBroadcast
app:ws("/notifications", {
    open = function(conn) conn:send("subscribed") end,
})

app:get("/announce", function(ctx)
    local count = app:wsBroadcast("/notifications", ctx.query.text or "ping")
    ctx:json({ delivered = count })
end)

app:get("/", function(ctx)
    ctx:html([[
<!doctype html>
<h1>WebSocket chat</h1>
<input id="msg"><button onclick="send()">send</button>
<pre id="log"></pre>
<script>
const ws = new WebSocket("ws://" + location.host + "/chat");
const log = document.getElementById("log");
ws.onmessage = e => log.textContent += e.data + "\n";
function send() { ws.send(document.getElementById("msg").value); }
</script>
]])
end)

app:listen({
    host = "0.0.0.0",
    port = tonumber(os.getenv("VARN_PORT") or "3000"),
})
