# 🧩 api

The public C embedding surface — [`varn.h`](include/varn/varn.h) — that wraps the runtime so a host
application written in C, C++, Swift, Kotlin, or anything with a C FFI can create a Varn instance, run
scripts, and shut it down without touching the internals. It has no Lua-facing API; it is the door from
native code into Varn, and the surface the mobile (`.xcframework`, `.aar`) and desktop embeddings build
on.

```c
#include <varn/varn.h>

varn_runtime* rt = varn_runtime_new();
varn_runtime_run_file(rt, "app.lua");
varn_runtime_free(rt);
```

## Capabilities

| Function | What it does |
|---|---|
| `varn_runtime_new()` | Create a runtime (a fresh Lua state with every native module installed). |
| `varn_runtime_run_file(rt, path)` | Run a Lua script file; returns a status code. |
| `varn_runtime_run_string(rt, source, chunk_name)` | Run a Lua chunk from a string. |
| `varn_runtime_stop(rt)` | Stop the runtime's event loop. |
| `varn_runtime_free(rt)` | Stop and destroy the runtime, releasing all resources. |
| `varn_version()` | The Varn version string. |

`VARN_API` controls symbol visibility (exported by default, `__declspec(dllexport)` under `VARN_SHARED`
on Windows).

## Reference

- Header: [include/varn/varn.h](include/varn/varn.h)
- Implementation: [src/Api.cpp](src/Api.cpp)
