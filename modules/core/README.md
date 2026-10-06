# 🧠 core

The runtime that hosts everything else, in two layers. The scheduler, the shared logging pipeline and TLS belong to the library `varn-runtime`, which links no Lua, so a C++ program uses them on their own. The Lua runtime, the Lua engine and the native module registry belong to the library `varn-core` on top of it. It has no Lua-facing API of its own — it is what loads and runs a script and wires every other module into the Lua state.

## What it contains

| Piece | Role |
|---|---|
| `runtime/Scheduler` | Owns the event loop, the task pool, the I/O pool and the ledger of the work in flight, without Lua, and advances them under the polls of a host. |
| `runtime/App`, `runtime/Runtime` | Own the `lua_State` on a scheduler and the lifecycle, plus the multi-process worker model (`VARN_WORKERS` + `SO_REUSEPORT`). |
| `runtime/EventLoop` | Waits on libuv (epoll/kqueue/IOCP) so socket and timer readiness resume coroutines inline. In the browser it is pumped manually. |
| `runtime/TaskPool` | Runs background jobs off the loop thread. |
| `lua/LuaEngine`, `lua/LuaHelpers` | Set up and drive the Lua state and the C++↔Lua bridge. |
| `lua/NativeModules` | Installs every native module (`fs`, `crypto`, `http`, `socket`, …) into the Lua state at startup. |
| `log/Log` | The shared logging sink the `log` module renders through. |
| `tls/CaBundle`, `tls/SystemTrust` | Load the trust store of the system into the TLS contexts of the network. |

Backed by Lua 5.5 — built as C++ so a raised Lua error unwinds as an exception instead of a `longjmp` across the embedding frames — and by libuv for the event loop.

## Reference

- Source: [src/](src/)
- Public embedding API: [../api](../api)
