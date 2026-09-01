# 🔌 socket

Async TCP, TLS, UDP, and unix-domain sockets. Every operation returns a promise.

## TCP

- `socket.tcp.connect(host, port, timeoutMs?)` → promise resolving to a socket. With `timeoutMs` set, a connect that does not complete in time rejects instead of waiting for the OS connect timeout; without it the OS default applies.
- `socket.tcp.listen(host, port, backlog?)` → promise resolving to a listener (`backlog` defaults to `64`).
- Socket: `sock:send(data)`, `sock:receive(maxBytes?)` (default `65536`), and `sock:close()` all return promises.
- Listener: `listener:accept()` (resolves to a socket) and `listener:close()`.

## TLS

- `socket.tls.connect(host, port, opts?)` → promise resolving to a secure socket with the same `send`/`receive`/`close` surface as a TCP socket. The connection completes the TLS handshake before resolving.
- `sock:startTls(host, opts?)` → upgrade an already-connected plaintext TCP socket to TLS in place, for protocols that negotiate TLS mid-stream (such as MySQL); resolves once the handshake completes. `opts` takes `insecure = true` to skip certificate verification.
- `opts` is an optional table: `timeoutMs` bounds the connect like the TCP variant, and `insecure = true` skips certificate verification.
- Certificates are verified against the system trust store by default; an invalid certificate rejects the connect. Use `insecure = true` only for self-signed endpoints under your control.
- TLS requires a build with TLS enabled; otherwise `socket.tls.connect` rejects with a clear error.
- On a secure socket the reads, writes and the handshake are serialized per connection, so overlapping operations on the same TLS socket run in order rather than in parallel; issue a concurrent send and receive on separate connections if you need true full-duplex.

## Unix-domain

- `socket.unix.connect(path)` → promise resolving to a socket connected to the filesystem path, with the same `send`/`receive`/`close` surface.
- `socket.unix.listen(path, backlog?)` → promise resolving to a listener (`backlog` defaults to `64`) bound to the path. The path must not already exist; remove a stale socket file before listening.
- Listener: `listener:accept()` (resolves to a socket) and `listener:close()`.

## UDP

- `socket.udp.bind(host, port)` → promise resolving to a UDP socket bound to the address.
- UDP socket: `sock:sendTo(host, port, data)`, `sock:recvFrom(maxBytes?)` (default `65536`), and `sock:close()` all return promises.
- `recvFrom` resolves to a table `{ data, host, port }` carrying the payload and the sender's address.

## Closing and limits

- `close()` is safe to call while a `receive`/`recvFrom`/`accept` is still pending. It signals the socket and returns promptly instead of blocking; the pending operation is released on its next internal poll (within ~200 ms).
- Once a socket is closed, a pending `receive`/`recvFrom` resolves to an empty/last result and a pending `accept` rejects with a "listener was closed" error.
- `receive` and `recvFrom` cap the read buffer (16 MB for TCP, 64 KB for UDP) regardless of the requested `maxBytes`, so an oversized request cannot drive a huge allocation.
- `host`/`port` are validated before use: the port must be in `1..65535` (the full integer is checked, not a truncated value).

## Examples

### `echo_server.lua`

```lua
-- echo service with an overridable listen port through the environment

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

### `udp_echo.lua`

```lua
-- udp echo service with an overridable bind address through the environment

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

### `unix_echo.lua`

```lua
-- unix-domain echo service over a filesystem path

local async = require("async")
local socket = require("socket")

local path = os.getenv("VARN_SOCKET_PATH") or "/tmp/varn-echo.sock"
os.remove(path)

async.spawn(function()
    local listener, lerr = socket.unix.listen(path, 128):await()
    if lerr then
        error(lerr)
    end
    print("unix echo listening on " .. path)
    while true do
        local client, aerr = listener:accept():await()
        if aerr then
            print("accept error:", aerr)
            break
        end
        async.spawn(function()
            local chunk = client:receive(4096):await()
            client:send("You sent: " .. chunk):await()
            client:close():await()
        end)
    end
    listener:close():await()
end)
```

### `tls_client.lua`

```lua
-- verified tls connection speaking a minimal http request over it

local async = require("async")
local socket = require("socket")

async.run(function()
    local conn, cerr = socket.tls.connect("example.com", 443, { timeoutMs = 5000 }):await()
    if cerr then
        error(cerr)
    end
    conn:send("GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n"):await()
    local reply = conn:receive(512):await()
    print("tls status:", reply:match("^[^\r\n]*"))
    conn:close():await()
end)
```

## Under the hood

Built on the Poco C++ networking libraries, with every socket multiplexed on a single event-driven I/O thread so a blocked accept or receive never ties up a worker. TLS connections use Poco's `SecureStreamSocket`, sharing the same non-blocking send/receive code path as plaintext TCP once the handshake completes.
