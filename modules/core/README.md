# 🧠 core

The runtime that hosts everything else: the Lua engine, the event loop, the native module registry, the
task pool, and the shared logging pipeline. It has no Lua-facing API of its own — it is what loads and
runs a script and wires every other module into the Lua state.

## What it contains

| Piece | Role |
|---|---|
| `runtime/App`, `runtime/Runtime` | Own the `lua_State` and the lifecycle, plus the multi-process worker model (`VARN_WORKERS` + `SO_REUSEPORT`). |
| `runtime/EventLoop` | Waits on libuv (epoll/kqueue/IOCP) so socket and timer readiness resume coroutines inline; in the browser it is pumped manually. |
| `runtime/TaskPool` | Runs background jobs off the Lua thread. |
| `lua/LuaEngine`, `lua/LuaHelpers` | Set up and drive the Lua state and the C++↔Lua bridge. |
| `lua/NativeModules` | Installs every native module (`fs`, `crypto`, `http`, `socket`, …) into the Lua state at startup. |
| `log/Log` | The shared logging sink the `log` module renders through. |

Backed by Lua 5.5 — built as C++ so a raised Lua error unwinds as an exception instead of a `longjmp`
across the embedding frames — and by libuv for the event loop.

## Reference

- Source: [src/](src/)
- Public embedding API: [../api](../api)
