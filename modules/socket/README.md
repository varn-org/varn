# 🔌 socket

Async TCP, TLS, UDP, and unix-domain sockets — every operation returns a promise, multiplexed on a
single event-driven I/O thread.

```lua
local socket = require("socket")
local conn = socket.tcp.connect("127.0.0.1", 9000):await()
```

## Capabilities

| Function | What it does |
|---|---|
| `socket.tcp.connect(host, port, timeoutMs?)` | Connect a TCP socket; with `timeoutMs` a stalled connect rejects instead of waiting for the OS timeout. |
| `socket.tcp.listen(host, port, backlog?)` | Bind a TCP listener (`backlog` defaults to `64`). |
| `socket.tls.connect(host, port, opts?)` | Connect a TLS socket; resolves after the handshake. `opts` takes `timeoutMs` and `insecure = true` to skip cert verification. |
| `socket.unix.connect(path)` | Connect a socket to a unix-domain filesystem path. |
| `socket.unix.listen(path, backlog?)` | Bind a unix-domain listener (`backlog` defaults to `64`); the path must not already exist. |
| `socket.udp.bind(host, port)` | Bind a UDP socket to the address. |
| `sock:send(data)` | Send bytes over a TCP/TLS/unix socket. |
| `sock:receive(maxBytes?)` | Receive up to `maxBytes` bytes (default `65536`, capped at 16 MB). |
| `sock:startTls(host, opts?)` | Upgrade an already-connected TCP socket to TLS in place, for protocols that negotiate TLS mid-stream (such as MySQL); `opts` takes `insecure = true` to skip cert verification. |
| `sock:sendTo(host, port, data)` | Send a UDP datagram to an address. |
| `sock:recvFrom(maxBytes?)` | Receive a UDP datagram (default `65536`, capped at 64 KB); resolves to `{ data, host, port }`. |
| `listener:accept()` | Accept the next incoming connection, resolving to a socket. |
| `sock:close()` / `listener:close()` | Close the socket or listener; safe to call while an operation is pending. |

On a TLS socket the reads, writes and the handshake are serialized per connection, so overlapping operations on one secure socket run in order rather than in parallel; a plaintext socket has no such restriction.

## Availability

Native on every desktop and mobile platform. Sockets need raw TCP/TLS/UDP and the ability to host a
listener, which a browser page cannot do, so the module is **native-only** — unavailable in the browser
(wasm). See the [platform matrix](../../docs/platform-availability.md).

## Reference, examples, and tests

- Full reference: [docs/lua-api/socket.md](../../docs/lua-api/socket.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
