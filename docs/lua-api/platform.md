# 🖥️ platform

Host and build information.

- `platform.os()` → e.g. `"linux"`, `"macos"`, `"windows"`, `"ios"`, `"android"`, `"wasm"`.
- `platform.arch()` → e.g. `"arm64"`, `"x86_64"`, `"wasm32"`.
- `platform.hostVersion()` → the Varn version string, for example `"0.0.1"`.
- `platform.version` → the same version already split, as `{ major, minor, patch, string }`. The three numbers are integers, so a component can gate on the runtime it needs without parsing the string and without inventing its own comparison:

  ```lua
  local v = require("platform").version
  if v.major > 1 or (v.major == 1 and v.minor >= 2) then
      -- a feature that landed in 1.2 is available
  end
  ```
- `platform.cpuCount()` → the number of CPUs.
- `platform.pointerSize()` → `4` or `8`.
- `platform.endianness()` → `"little"` or `"big"`.
- `platform.libPrefix()` and `platform.shlibSuffix()` → the shared-library naming pieces.
- `platform.libraryFilename(name)` → e.g. `libz.so` for `"z"`.
- `platform.getLibraryPathByName(name, subdir?)` → builds `subdir/filename` (a dev helper).

## Examples

### `host_version.lua`

```lua
-- prints semver baked in at configure time for the host binary
local p = require("platform")

local v = p.hostVersion()
print("hostVersion", v)
assert(type(v) == "string" and #v > 0, "hostVersion")
assert(v:match("^%d+%.%d+%.%d+"), "expected semver x.y.z")
```

### `info.lua`

```lua
-- prints host identifiers, system data, and shared library naming hints
local p = require("platform")

print("os()         ", p.os())
print("arch()       ", p.arch())
print("hostVersion()", p.hostVersion())
print("cpuCount()   ", p.cpuCount())
print("pointerSize()", p.pointerSize())
print("endianness() ", p.endianness())
print("libPrefix()  ", p.libPrefix())
print("shlibSuffix()", p.shlibSuffix())

assert(type(p.os()) == "string" and #p.os() > 0, "os")
assert(type(p.arch()) == "string", "arch")
assert(p.cpuCount() >= 1, "cpuCount")
assert(p.pointerSize() == 4 or p.pointerSize() == 8, "pointerSize")
assert(p.endianness() == "little" or p.endianness() == "big", "endianness")

print("platform info ok")
```

### `library_paths.lua`

```lua
-- builds example filenames for a short logical library name
local p = require("platform")

local name = "z"
local fn = p.libraryFilename(name)
print("libraryFilename('z') =", fn)

local rel = p.getLibraryPathByName(name, "vendor/libs")
print("getLibraryPathByName('z', 'vendor/libs') =", rel)

assert(fn:match("%."), "expected extension in filename")
print("platform library path helpers ok")
```

## Under the hood

Reads the host operating-system and build APIs directly, with no external dependency.
