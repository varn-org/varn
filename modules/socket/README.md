# 🔌 socket

Async TCP, TLS, UDP, and Unix-domain sockets — every operation returns a promise, multiplexed on the event loop, with host names resolved on the I/O pool.

```lua
local socket = require("socket")
local conn = socket.tcp.connect("127.0.0.1", 9000):await()
```

## Capabilities

| Function | What it does |
|---|---|
| `socket.resolve(host)` | Resolve a host to the list of its addresses on the I/O pool. |
| `socket.tcp.connect(host, port, opts?)` | Connect a TCP socket. The table `opts` takes `timeoutMs`, which rejects a stalled connect instead of waiting for the OS timeout, and `maxQueuedBytes`. |
| `socket.tcp.listen(host, port, opts?)` | Bind a TCP listener, on a free port for port `0`. The table `opts` takes `backlog` (default `64`), `reusePort` and `maxQueuedBytes`. A busy port fails by its address unless every listener sets `reusePort`. |
| `socket.tls.connect(host, port, opts?)` | Connect a TLS socket. Resolves after the handshake. The table `opts` takes `timeoutMs`, `insecure = true` to skip cert verification and `maxQueuedBytes`. |
| `socket.unix.connect(path, opts?)` | Connect a socket to a Unix-domain filesystem path. |
| `socket.unix.listen(path, opts?)` | Bind a Unix-domain listener (`backlog` defaults to `64`). The path must not already exist. |
| `socket.udp.bind(host, port)` | Bind a UDP socket to the address, on a free port for port `0`. |
| `sock:send(data)` | Send bytes over a TCP/TLS/Unix socket. A send that would queue past `maxQueuedBytes` rejects. |
| `sock:receive(maxBytes?)` | Receive up to `maxBytes` bytes (default `65536`, capped at 16 MB). Resolves to `""` once the peer closed the stream. |
| `sock:shutdown("send")` | Shut the sending side down after the queued sends while the socket still receives. |
| `sock:setNoDelay(enabled)` / `sock:setKeepAlive(enabled)` | Set `TCP_NODELAY` or `SO_KEEPALIVE`. |
| `sock.localHost` / `sock.localPort` / `sock.peerHost` / `sock.peerPort` / `sock.pendingBytes` | The two ends of the connection and the bytes queued for sending. |
| `sock:startTls(host, opts?)` | Upgrade an already-connected TCP socket to TLS in place, for protocols that negotiate TLS mid-stream (such as MySQL). The table `opts` takes `insecure = true` to skip cert verification. |
| `sock:sendTo(host, port, data)` | Send a UDP datagram to an address. |
| `sock:recvFrom(maxBytes?)` | Receive a UDP datagram (default `65536`, capped at 64 KB). Resolves to `{ data, host, port }`. |
| `sock:sendMany(list)` | Send a list of `{ host, port, data }` datagrams in order, resolving to how many were sent. |
| `sock:recvMany(maxDatagrams?, maxBytes?)` | Wait for a datagram, then resolve to the list of every datagram already waiting, up to `maxDatagrams` (default `64`). |
| `sock:connect(host, port)` / `sock:send(data)` | Set the only peer of a UDP socket and send datagrams to it. |
| `sock:setBroadcast(enabled)` | Allow datagrams to a broadcast address. |
| `sock:joinGroup(address)` / `sock:leaveGroup(address)` | Join or leave a multicast group. |
| `listener.host` / `listener.port` / `udp.host` / `udp.port` | The address a listener or a UDP socket bound. |
| `listener:accept()` | Accept the next incoming connection, resolving to a socket. |
| `sock:close()` / `listener:close()` | Close the socket or listener. A pending operation rejects with `The socket was closed.` or `The listener was closed.`. |

On a TLS socket the reads, writes and the handshake are serialized per connection, so overlapping operations on one secure socket run in order rather than in parallel. A plaintext socket has no such restriction.

## Availability

Native on every desktop and mobile platform. Sockets need raw TCP/TLS/UDP and the ability to host a listener, which a browser page cannot do, so the module is **native-only** — unavailable in the browser (wasm). See the [platform matrix](../../docs/platform-availability.md).

## Reference and tests

- Full reference: [docs/lua-api/socket.md](../../docs/lua-api/socket.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
