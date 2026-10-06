# 🔌 socket

Async TCP, TLS, UDP, and Unix-domain sockets. Every operation returns a promise.

## Names and addresses

- `socket.resolve(host)` → A promise resolving to the list of every address of the host as strings, in the order the resolver of the system answers them. A numeric address resolves to itself. A host that does not exist rejects with `The host "name" could not be resolved.`.
- A host name given to any call is looked up on the I/O pool, so a connect, a listen, a bind or a datagram never blocks the loop thread while a resolver answers. A numeric address is used as it is. A name resolves to the address the system prefers, which can be IPv6, so a listener and a client meant to meet bind and connect through the same name or the same numeric address.

## TCP

- `socket.tcp.connect(host, port, opts?)` → A promise resolving to a socket. The table `opts` takes `timeoutMs` (from `0` to `3600000`), which bounds the lookup of the name and the connect together and rejects with `The connection to host:port timed out.` instead of waiting for the system timeout, and `maxQueuedBytes`.
- `socket.tcp.listen(host, port, opts?)` → A promise resolving to a listener. The port may be `0`, which binds a free port the listener then reports. The table `opts` takes `backlog` (from `1` to `4096`, default `64`), `reusePort` and `maxQueuedBytes`, which every socket the listener accepts takes.
- A listener binds without `SO_REUSEPORT`, so a second listener on a busy port fails with `The address 127.0.0.1:41000 is already in use.` instead of sharing it. On POSIX it binds with `SO_REUSEADDR`, so a restarted program binds a port whose connections are still in `TIME_WAIT`, which Windows allows on its own, and on Windows it binds with `SO_EXCLUSIVEADDRUSE`, so no other socket can take the port over.
- With `reusePort = true` the listener binds with `SO_REUSEPORT`, and listeners that all ask for it share one port while the system spreads the connections across them. Windows has no such option, so there the listen rejects with `The option "reusePort" is not available on Windows.`.
- Listener: `listener:accept()` (resolves to a socket) and `listener:close()`, and the fields `listener.host` and `listener.port` hold the address it bound.
- Socket: `sock:send(data)`, `sock:receive(maxBytes?)` (default `65536`), `sock:shutdown("send")`, `sock:setNoDelay(enabled)`, `sock:setKeepAlive(enabled)` and `sock:close()` all return promises.
- The method `send` resolves once the system took every byte, so an awaited send applies backpressure. The bytes of the sends not yet handed to the system are counted in `sock.pendingBytes`, and a send that would raise that count past `maxQueuedBytes` (default `67108864`, 64 MiB) rejects at once with `The socket already queues 4194304 bytes, so a send of 1048576 bytes would pass its limit of 4194304 bytes.` while the queued sends still leave.
- The method `receive` resolves to the empty string once the peer closed its side of the stream.
- `sock:shutdown("send")` shuts the sending side down once every send queued before it left, so the peer reads the end of the stream while this socket still receives. A send after it rejects with `The socket was shut down for sending.`. The argument names the side, and only `"send"` is accepted.
- `sock:setNoDelay(enabled)` sets `TCP_NODELAY`, which turns Nagle's algorithm off, and `sock:setKeepAlive(enabled)` sets `SO_KEEPALIVE`. Both take a boolean and resolve to `"ok"`.
- The fields `sock.localHost`, `sock.localPort`, `sock.peerHost` and `sock.peerPort` hold the two ends of the connection.

## TLS

- `socket.tls.connect(host, port, opts?)` → A promise resolving to a secure socket with the same methods and fields as a TCP socket. The connection completes the TLS handshake before resolving.
- `sock:startTls(host, opts?)` → Upgrade an already-connected plaintext TCP socket to TLS in place, for protocols that negotiate TLS mid-stream (such as MySQL). Resolves once the handshake completes. The table `opts` takes `insecure = true` to skip certificate verification.
- The argument `opts` is an optional table: `timeoutMs` bounds the connect like the TCP variant, `insecure = true` skips certificate verification and `maxQueuedBytes` bounds the queued sends.
- Certificates are verified against the system trust store by default. An invalid certificate rejects the connect with `The TLS handshake with host failed: ...`. Use `insecure = true` only for self-signed endpoints under your control.
- TLS requires a build with TLS enabled. Otherwise `socket.tls.connect` rejects with a clear error.
- On a secure socket the reads, writes, the shutdown and the handshake are serialized per connection, so overlapping operations on the same TLS socket run in order rather than in parallel. Issue a concurrent send and receive on separate connections if you need true full-duplex.

## Unix-domain

- `socket.unix.connect(path, opts?)` → A promise resolving to a socket connected to the filesystem path, with the same methods as a TCP socket. The table `opts` takes `maxQueuedBytes`.
- `socket.unix.listen(path, opts?)` → A promise resolving to a listener bound to the path. The table `opts` takes `backlog` (default `64`) and `maxQueuedBytes`. The path must not already exist. Remove a stale socket file before listening.
- On a Unix-domain socket or listener the host fields hold the path and the port fields are `nil`.

## UDP

- `socket.udp.bind(host, port)` → A promise resolving to a UDP socket bound to the address. The port may be `0`, and the fields `udp.host` and `udp.port` hold the address it bound. A UDP socket binds without `SO_REUSEADDR` or `SO_REUSEPORT`, since on Linux either lets a second socket share a busy port, and with `SO_EXCLUSIVEADDRUSE` on Windows, so a busy port fails with `The address 127.0.0.1:41000 is already in use.`.
- UDP socket: `sock:sendTo(host, port, data)`, `sock:recvFrom(maxBytes?)` (default `65536`), and `sock:close()` all return promises. A datagram addressed by a name leaves once the name resolved, so it may leave after a later one addressed by a number.
- The method `recvFrom` resolves to a table `{ data, host, port }` carrying the payload and the sender's address.
- `sock:connect(host, port)` → Set the only peer the socket sends to with `send` and receives from. `sock:send(data)` → Send a datagram to that peer, which rejects with `The socket is not connected. Connect it with "connect" before calling "send".` before a connect. A connected socket sends with `send`, since some systems refuse a `sendTo` on it.
- `sock:sendMany(list)` → Send a list of datagrams, each a table `{ host, port, data }`, in order, and resolve to how many were sent once every one left. A malformed entry raises before anything is sent, naming its index, a name that does not resolve rejects with its index before anything is sent, and a datagram that fails rejects with its index.
- `sock:recvMany(maxDatagrams?, maxBytes?)` → Wait for a datagram, then resolve to a list of every datagram already waiting, up to `maxDatagrams` (default `64`), each a table `{ data, host, port }` of at most `maxBytes` bytes (default `65536`). A game reads the whole burst of a frame in one call instead of one datagram per await.
- `sock:setBroadcast(enabled)` → Set `SO_BROADCAST`, without which a datagram to a broadcast address such as `255.255.255.255` is refused by the system.
- `sock:joinGroup(address)` and `sock:leaveGroup(address)` → Join or leave a multicast group, given as a numeric IPv4 or IPv6 group address, on the interface the system picks for it. Bind the socket to the wildcard address of its family to receive what the group carries. An address that is not a group rejects with `The address "x" is not a multicast group.`.

## Serving ready sockets

A socket that is ready serves every operation that waits on it, in order, until one would block, so several pending receives, sends or accepts on one socket all complete in the same turn of the loop. The HTTP server likewise reads a connection until it would block, up to a bound per turn. A host that polls the runtime with a budget repeats those turns while they make progress, so a task that reads again after a receive reads in the same poll rather than in the next frame.

## Errors

- A failure names the address it concerns and carries the text of the error and the system error, such as `The connection to 127.0.0.1:9 was refused.`, `The connection to 192.0.2.1:80 timed out.` or `The send to 127.0.0.1:5000 failed: Connection reset by peer (system error 54).`. The number of a system error differs between systems.
- A mistaken argument, such as a port outside its range or an option of the wrong type, raises at once instead of rejecting.

## Closing and limits

- Calling `close()` is safe while an operation is still pending. A socket or listener is marked closed before its pending operations settle, so a pending `accept` rejects with `The listener was closed.` and a pending `receive`, `send`, `recvFrom` or `recvMany` rejects with `The socket was closed.`. An operation started after the close rejects the same way.
- The empty string stays the end of a stream the peer closed, so a `receive` tells a local close, which rejects, from the end of the data, which resolves to `""`.
- The methods `receive` and `recvFrom` cap the read buffer (16 MB for TCP, 64 KB for UDP) regardless of the requested `maxBytes`, so an oversized request cannot drive a huge allocation.
- The methods `sendMany` and `recvMany` take at most 1024 datagrams per call.
- The arguments `host`/`port` are validated before use: a port to connect or send to must be in `1..65535`, a port to listen or bind in `0..65535`, and the full integer is checked, not a truncated value.

## Examples

### Closing a pending operation

```lua
-- Closing a socket rejects a pending receive with the close, closing a listener rejects a pending accept, and the peer reads the end of the stream.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"

async.run(function()
    -- A listener with no incoming connection keeps accept pending until closed.
    local listener = socket.tcp.listen(host, 0):await()
    async.spawn(function()
        async.sleep(80):await()
        listener:close():await()
    end)
    print("accept after close:", listener:accept():await())

    -- A connected socket with an idle peer keeps receive pending until closed.
    local server = socket.tcp.listen(host, 0):await()
    local conn = socket.tcp.connect(host, server.port):await()
    local peer = server:accept():await()
    async.spawn(function()
        async.sleep(80):await()
        conn:close():await()
    end)
    print("receive after close:", conn:receive(4096):await())
    print("peer reads the end:", #peer:receive():await())
    peer:close():await()
    server:close():await()

    print("socket close pending ok")
end)
```

### Port 0 and a busy port

```lua
-- A listener on port 0 reports the free port it bound, and a second listener on that port fails by its address.
local async = require("async")
local socket = require("socket")

async.run(function()
    local listener = socket.tcp.listen("127.0.0.1", 0):await()
    print("listening on", listener.host, listener.port)

    local second, err = socket.tcp.listen("127.0.0.1", listener.port):await()
    print("second listener:", second, err)

    local udp = socket.udp.bind("127.0.0.1", 0):await()
    print("udp bound to", udp.host, udp.port)

    udp:close():await()
    listener:close():await()
    print("socket ports ok")
end)
```

### Names off the loop thread

```lua
-- A name resolves on the I/O pool for a lookup, a listener and a connect, so the loop never waits for a resolver.
local async = require("async")
local socket = require("socket")

async.run(function()
    print("localhost is", table.concat(socket.resolve("localhost"):await(), ", "))
    print("unknown host:", select(2, socket.resolve("host.invalid"):await()))

    local listener = socket.tcp.listen("localhost", 0):await()
    local conn = socket.tcp.connect("localhost", listener.port, { timeoutMs = 2000 }):await()
    local peer = listener:accept():await()
    print("connected", conn.localHost, conn.localPort, "to", conn.peerHost, conn.peerPort)

    conn:close():await()
    peer:close():await()
    listener:close():await()
    print("socket names ok")
end)
```

### Options, half close and queued sends

```lua
-- A client turns Nagle's algorithm off, bounds what it queues, and shuts its sending side down while it still reads the reply.
local async = require("async")
local socket = require("socket")

async.run(function()
    local listener = socket.tcp.listen("127.0.0.1", 0):await()
    local conn = socket.tcp.connect("127.0.0.1", listener.port, { maxQueuedBytes = 1024 }):await()
    local peer = listener:accept():await()

    conn:setNoDelay(true):await()
    conn:setKeepAlive(true):await()

    local queued = conn:send(string.rep("a", 1000))
    print("pending bytes:", conn.pendingBytes)
    print("over the bound:", select(2, conn:send(string.rep("b", 100)):await()))
    queued:await()

    -- The peer reads the request up to the end of the stream and answers on the side that is still open.
    conn:shutdown("send"):await()
    local request = ""
    while true do
        local chunk = peer:receive():await()
        if chunk == "" then
            break
        end
        request = request .. chunk
    end

    peer:send("got " .. #request .. " bytes"):await()
    print("reply:", conn:receive():await())

    conn:close():await()
    peer:close():await()
    listener:close():await()
    print("socket options ok")
end)
```

### Connected UDP, broadcast and multicast

```lua
-- A connected UDP socket sends with `send`, broadcast is turned on before a broadcast, and a socket joins and leaves a multicast group.
local async = require("async")
local socket = require("socket")

async.run(function()
    local server = socket.udp.bind("127.0.0.1", 0):await()
    local client = socket.udp.bind("127.0.0.1", 0):await()

    client:connect("127.0.0.1", server.port):await()
    client:send("hello"):await()
    local packet = server:recvFrom():await()
    print("server got", packet.data, "from port", packet.port)

    local lan = socket.udp.bind("0.0.0.0", 0):await()
    lan:setBroadcast(true):await()
    print("broadcast:", lan:sendTo("255.255.255.255", lan.port, "anyone there?"):await())
    print("join:", lan:joinGroup("239.255.42.99"):await())
    print("leave:", lan:leaveGroup("239.255.42.99"):await())

    lan:close():await()
    client:close():await()
    server:close():await()
    print("socket udp options ok")
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
    local listener, lerr = socket.tcp.listen(host, port, { backlog = 128 }):await()
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
-- An in-process TCP server echoes one message back to a client and both shut down.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local port = 9831

async.spawn(function()
    local listener = socket.tcp.listen(host, port, { backlog = 16 }):await()
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
-- Opens a verified TLS connection and speaks a minimal HTTP request over it.
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
-- UDP echo service with an overridable bind address through the environment.

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
-- An in-process UDP server echoes one datagram back and both sockets close.
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

### UDP batches

```lua
-- One socket sends a burst of datagrams in one call and the other reads every datagram that waits in one call.
local async = require("async")
local socket = require("socket")

local host = "127.0.0.1"
local serverPort = 9843
local clientPort = 9844

async.run(function()
    local server = socket.udp.bind(host, serverPort):await()
    local client = socket.udp.bind(host, clientPort):await()

    local burst = {}
    for i = 1, 10 do
        burst[i] = { host = host, port = serverPort, data = "state " .. i }
    end

    print("udp sent:", client:sendMany(burst):await())

    local received = 0
    while received < 10 do
        local datagrams = server:recvMany(64):await()
        received = received + #datagrams
        print("udp batch of", #datagrams, "starting with", datagrams[1].data)
    end

    server:close():await()
    client:close():await()
    print("socket udp batches ok")
end)
```

### Unix socket round trip

```lua
-- An in-process Unix-domain server echoes one message back to a client and both shut down.
local async = require("async")
local socket = require("socket")

local path = os.tmpname() .. ".sock"
os.remove(path)

async.spawn(function()
    local listener = socket.unix.listen(path, { backlog = 16 }):await()
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

Built on the Poco C++ networking libraries, with every plaintext socket multiplexed on the event loop so a blocked accept or receive never ties up a worker. Host names resolve on the I/O pool. TLS connections use Poco's `SecureStreamSocket` and run their handshake, reads and writes blocking on the I/O pool, one operation at a time per connection.
