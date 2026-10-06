# 🖥️ platform

Host and build information — operating system, CPU architecture, build version, and shared-library naming pieces — read directly from the host OS and build APIs with no external dependency.

```lua
local platform = require("platform")
print(platform.os(), platform.arch())
```

## Capabilities

| Function | What it does |
|---|---|
| `platform.os()` | Host operating system identifier, e.g. `"linux"`, `"macos"`, `"windows"`, `"ios"`, `"android"`, `"wasm"`. |
| `platform.arch()` | CPU architecture identifier, e.g. `"arm64"`, `"x86_64"`, `"wasm32"`. |
| `platform.hostVersion()` | The Varn version string baked in at configure time. |
| `platform.version` | The same version already split as `{ major, minor, patch, string }`, with the three numbers as integers so a component can gate on the runtime without parsing. |
| `platform.cpuCount()` | The number of CPUs reported for the host. |
| `platform.pointerSize()` | Pointer width in bytes — `4` or `8`. |
| `platform.endianness()` | Host byte order — `"little"` or `"big"`. |
| `platform.libPrefix()` | The shared-library name prefix, e.g. `"lib"`. |
| `platform.shlibSuffix()` | The shared-library file suffix, e.g. `".so"`, `".dylib"`, `".dll"`. |
| `platform.libraryFilename(name)` | The full shared-library filename for a logical name, e.g. `libz.so` for `"z"`. |
| `platform.getLibraryPathByName(name, subdir?)` | Builds `subdir/filename` for a logical name — a dev helper. |

## Reference and tests

- Full reference: [docs/lua-api/platform.md](../../docs/lua-api/platform.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
