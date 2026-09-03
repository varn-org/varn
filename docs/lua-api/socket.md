# 🔌 socket

Async TCP, TLS, UDP, and unix-domain sockets. Every operation returns a promise.

## TCP

- `socket.tcp.connect(host, port, timeoutMs?)` → promise resolving to a socket. With `timeoutMs` set, a connect that does not complete in time rejects instead of waiting for the OS connect timeout. Without it the OS default applies.
- `socket.tcp.listen(host, port, backlog?)` → promise resolving to a listener (`backlog` defaults to `64`).
- Socket: `sock:send(data)`, `sock:receive(maxBytes?)` (default `65536`), and `sock:close()` all return promises.
- Listener: `listener:accept()` (resolves to a socket) and `listener:close()`.

## TLS

- `socket.tls.connect(host, port, opts?)` → promise resolving to a secure socket with the same `send`/`receive`/`close` surface as a TCP socket. The connection completes the TLS handshake before resolving.
- `sock:startTls(host, opts?)` → upgrade an already-connected plaintext TCP socket to TLS in place, for protocols that negotiate TLS mid-stream (such as MySQL). Resolves once the handshake completes. `opts` takes `insecure = true` to skip certificate verification.
- `opts` is an optional table: `timeoutMs` bounds the connect like the TCP variant, and `insecure = true` skips certificate verification.
- Certificates are verified against the system trust store by default. An invalid certificate rejects the connect. Use `insecure = true` only for self-signed endpoints under your control.
- TLS requires a build with TLS enabled. Otherwise `socket.tls.connect` rejects with a clear error.
- On a secure socket the reads, writes and the handshake are serialized per connection, so overlapping operations on the same TLS socket run in order rather than in parallel. Issue a concurrent send and receive on separate connections if you need true full-duplex.

## Unix-domain

- `socket.unix.connect(path)` → promise resolving to a socket connected to the filesystem path, with the same `send`/`receive`/`close` surface.
- `socket.unix.listen(path, backlog?)` → promise resolving to a listener (`backlog` defaults to `64`) bound to the path. The path must not already exist. Remove a stale socket file before listening.
- Listener: `listener:accept()` (resolves to a socket) and `listener:close()`.

## UDP

- `socket.udp.bind(host, port)` → promise resolving to a UDP socket bound to the address.
- UDP socket: `sock:sendTo(host, port, data)`, `sock:recvFrom(maxBytes?)` (default `65536`), and `sock:close()` all return promises.
- `recvFrom` resolves to a table `{ data, host, port }` carrying the payload and the sender's address.

## Closing and limits

- `close()` is safe to call while a `receive`/`recvFrom`/`accept` is still pending. It signals the socket and returns promptly instead of blocking. The pending operation is released on its next internal poll (within ~200 ms).
- Once a socket is closed, a pending `receive`/`recvFrom` resolves to an empty/last result and a pending `accept` rejects with a "listener was closed" error.
- `receive` and `recvFrom` cap the read buffer (16 MB for TCP, 64 KB for UDP) regardless of the requested `maxBytes`, so an oversized request cannot drive a huge allocation.
- `host`/`port` are validated before use: the port must be in `1..65535` (the full integer is checked, not a truncated value).

## Examples

### Closing a pending operation

```lua
-- Closing a socket releases a pending receive and a pending accept.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port = 9861

async.run(function()
    -- A listener with no incoming connection keeps accept pending until closed.
    local listener = socket.tcp.listen(host, port, 16):await()
    async.spawn(function()
        async.sleep(80):await()
        listener:close():await()
    end)
    local accepted, aerr = listener:accept():await()
    print("accept after close:", accepted, aerr)

    -- A connected socket with an idle peer keeps receive pending until closed.
    local server = socket.tcp.listen(host, port + 1, 16):await()
    local conn = socket.tcp.connect(host, port + 1):await()
    local peer = server:accept():await()
    async.spawn(function()
        async.sleep(80):await()
        conn:close():await()
    end)
    local chunk, rerr = conn:receive(4096):await()
    print("receive after close:", chunk and #chunk or chunk, rerr)
    peer:close():await()
    server:close():await()

    print("socket close pending ok")
end)
```

### Echo server

```lua
-- Echo service with an overridable listen port through the environment.

local async = require("async")
local socket = require("socket")

local host = os.getenv("VARN_SOCKET_HOST") or "127.0.0.1"
local port = tonumber(os.getenv("VARN_SOCKET_PORT") or "9000")

local function handleClient(sock)
    while true do
        local chunk, err = sock:receive(4096):await()
        if err then
            print("client receive error:", err)
            break
        end
        if #chunk == 0 then
            break
        end
        local _, sendErr = sock:send("You sent: " .. chunk):await()
        if sendErr then
            print("client send error:", sendErr)
            break
        end
    end
    sock:close():await()
end

async.spawn(function()
    local listener, lerr = socket.tcp.listen(host, port, 128):await()
    if lerr then
        error(lerr)
    end
    print(string.format("tcp echo listening on %s:%d", host, port))
    while true do
        local client, aerr = listener:accept():await()
        if aerr then
            print("accept error:", aerr)
            break
        end
        async.spawn(function()
            handleClient(client)
        end)
    end
    listener:close():await()
end)
```

### TCP round trip

```lua
-- An in-process tcp server echoes one message back to a client and both shut down.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port = 9831

async.spawn(function()
    local listener = socket.tcp.listen(host, port, 16):await()
    local client = listener:accept():await()
    local chunk = client:receive(4096):await()
    client:send("echo:" .. chunk):await()
    client:close():await()
    listener:close():await()
end)

async.run(function()
    async.sleep(80):await()

    local conn = socket.tcp.connect(host, port):await()
    conn:send("hello"):await()
    local reply = conn:receive(4096):await()
    print("tcp reply:", reply)
    conn:close():await()

    print("socket tcp round-trip ok")
end)
```

### TLS client

```lua
-- Opens a verified tls connection and speaks a minimal http request over it.
local async = require("async")
local socket = require("socket")

local host = os.getenv("VARN_TLS_HOST") or "example.com"
local port = tonumber(os.getenv("VARN_TLS_PORT") or "443")

async.run(function()
    local conn, cerr = socket.tls.connect(host, port, { timeoutMs = 5000 }):await()
    if cerr then
        print("tls connect error:", cerr)
        return
    end

    local request = "GET / HTTP/1.0\r\nHost: " .. host .. "\r\nConnection: close\r\n\r\n"
    conn:send(request):await()

    local reply, rerr = conn:receive(512):await()
    if rerr then
        print("tls receive error:", rerr)
    else
        local statusLine = reply:match("^[^\r\n]*")
        print("tls status:", statusLine)
    end

    conn:close():await()

    print("socket tls client ok")
end)
```

### UDP echo

```lua
-- Udp echo service with an overridable bind address through the environment.

local async = require("async")
local socket = require("socket")

local host = os.getenv("VARN_SOCKET_HOST") or "127.0.0.1"
local port = tonumber(os.getenv("VARN_SOCKET_PORT") or "9000")

async.spawn(function()
    local sock, berr = socket.udp.bind(host, port):await()
    if berr then
        error(berr)
    end
    print(string.format("udp echo listening on %s:%d", host, port))
    while true do
        local packet, rerr = sock:recvFrom(4096):await()
        if rerr then
            print("recv error:", rerr)
            break
        end
        local _, serr = sock:sendTo(packet.host, packet.port, "You sent: " .. packet.data):await()
        if serr then
            print("send error:", serr)
            break
        end
    end
    sock:close():await()
end)
```

### UDP round trip

```lua
-- An in-process udp server echoes one datagram back and both sockets close.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local serverPort = 9841
local clientPort = 9842

async.spawn(function()
    local server = socket.udp.bind(host, serverPort):await()
    local packet = server:recvFrom(4096):await()
    server:sendTo(packet.host, packet.port, "echo:" .. packet.data):await()
    server:close():await()
end)

async.run(function()
    async.sleep(80):await()

    local client = socket.udp.bind(host, clientPort):await()
    client:sendTo(host, serverPort, "hello"):await()
    local reply = client:recvFrom(4096):await()
    print("udp reply:", reply.data, "from", reply.host, reply.port)
    client:close():await()

    print("socket udp round-trip ok")
end)
```

### Unix socket round trip

```lua
-- An in-process unix-domain server echoes one message back to a client and both shut down.
local async = require("async")
local socket = require("socket")

local path = os.tmpname() .. ".sock"
os.remove(path)

async.spawn(function()
    local listener = socket.unix.listen(path, 16):await()
    local client = listener:accept():await()
    local chunk = client:receive(4096):await()
    client:send("echo:" .. chunk):await()
    client:close():await()
    listener:close():await()
end)

async.run(function()
    async.sleep(80):await()

    local conn = socket.unix.connect(path):await()
    conn:send("hello"):await()
    local reply = conn:receive(4096):await()
    print("unix reply:", reply)
    conn:close():await()
    os.remove(path)

    print("socket unix round-trip ok")
end)
```
## Under the hood

Built on the Poco C++ networking libraries, with every socket multiplexed on a single event-driven I/O thread so a blocked accept or receive never ties up a worker. TLS connections use Poco's `SecureStreamSocket`, sharing the same non-blocking send/receive code path as plaintext TCP once the handshake completes.
