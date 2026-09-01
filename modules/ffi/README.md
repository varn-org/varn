# 🧩 ffi

Declare C functions, structs, and types and call native libraries directly from Lua — backed by libffi.

```lua
local ffi = require("ffi")
ffi.cdef [[ unsigned long strlen(const char *s); ]]
print(ffi.C.strlen("hello")) -- 5
```

## Capabilities

| Function | What it does |
|---|---|
| `ffi.cdef(declarations)` | Register C declarations (functions, typedefs, structs) for later use. |
| `ffi.C.<name>(...)` | Call a function from the default process namespace. |
| `ffi.load(name)` | Load a shared library and return a namespace for its declared symbols (resolve the platform name with `platform.libraryFilename`). |
| `ffi.new(ct [, n] [, init...])` | Allocate a cdata object of C type `ct`, such as `"sqlite3*[1]"` or `"unsigned char[?]"`. |
| `ffi.cast(ct, value)` | Reinterpret a value or pointer as C type `ct`. |
| `ffi.string(ptr [, len])` | Copy a C string, or `len` raw bytes, into a Lua string. |
| `ffi.copy(dst, src [, len])` | Copy bytes into a cdata buffer. |
| `ffi.fill(dst, len [, value])` | Set `len` bytes of a cdata buffer to `value` (default `0`). |
| `ffi.sizeof(ct [, n])` | Byte size of a C type or cdata. |
| `ffi.typeof(ct)` | Build a reusable ctype from a declaration string. |
| `ffi.offsetof(ct, field)` | Byte offset of a struct field. |
| `ffi.metatype(ct, metatable)` | Attach a Lua metatable to a ctype so its instances gain methods and operators. |
| `ffi.istype(ct, value)` | Whether `value` is a cdata of C type `ct`. |
| `ffi.gc(cdata, finalizer)` | Register a finalizer to run when the cdata is collected; returns the cdata. |
| `ffi.tonumber(value)` | Convert a cdata scalar to a Lua number. |
| `ffi.addressof(cdata)` | A pointer to a live cdata object. |
| `ffi.errno([value])` | Read (or set) the C `errno` value. |
| `ffi.nullptr` | The null pointer value. |
| `ffi.VERSION` | The FFI backend version string. |

This calls arbitrary native code by design; treat declarations and inputs as trusted.

## Availability

`ffi` calls native libraries through libffi and is native on every desktop and mobile platform. In
the browser (wasm) it is unavailable — wasm has no native calling convention to target — so the module
still loads but its calls return a clear error. See the
[platform matrix](../../docs/platform-availability.md).

## Reference, examples, and tests

- Full reference: [docs/lua-api/ffi.md](../../docs/lua-api/ffi.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
