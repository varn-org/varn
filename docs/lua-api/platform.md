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

### Byte layout

```lua
-- Prints the pointer size and byte order of the host build.
local p = require("platform")

print("pointerSize()", p.pointerSize())
print("endianness() ", p.endianness())

assert(p.pointerSize() == 4 or p.pointerSize() == 8, "pointerSize")
assert(p.endianness() == "little" or p.endianness() == "big", "endianness")
print("platform byte layout ok")
```

### CPU count

```lua
-- Prints the number of cpus reported for the host.
local p = require("platform")

local cpus = p.cpuCount()
print("cpuCount()", cpus)

assert(type(cpus) == "number" and cpus >= 1, "cpuCount should be at least one")
print("platform cpu count ok")
```

### Host version

```lua
-- Prints semver baked in at configure time for the host binary.
local p = require("platform")

local v = p.hostVersion()
print("hostVersion", v)
assert(type(v) == "string" and #v > 0, "hostVersion")
assert(v:match("^%d+%.%d+%.%d+"), "expected semver x.y.z")
```

### Info

```lua
-- Prints host identifiers, system data, and shared library naming hints.
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

### Library naming

```lua
-- Prints the shared library prefix and suffix pieces for the host.
local p = require("platform")

local prefix = p.libPrefix()
local suffix = p.shlibSuffix()
print("libPrefix()  ", prefix)
print("shlibSuffix()", suffix)

assert(type(prefix) == "string", "libPrefix")
assert(type(suffix) == "string" and #suffix > 0, "shlibSuffix")
print("platform lib naming ok")
```

### Library paths

```lua
-- Builds example filenames for a short logical library name.
local p = require("platform")

local name = "z"
local fn = p.libraryFilename(name)
print("libraryFilename('z') =", fn)

local rel = p.getLibraryPathByName(name, "vendor/libs")
print("getLibraryPathByName('z', 'vendor/libs') =", rel)

assert(fn:match("%."), "expected extension in filename")
print("platform library path helpers ok")
```

### System identity

```lua
-- Prints the host operating system and cpu architecture identifiers.
local p = require("platform")

print("os()  ", p.os())
print("arch()", p.arch())

assert(type(p.os()) == "string" and #p.os() > 0, "os")
assert(type(p.arch()) == "string" and #p.arch() > 0, "arch")
print("platform system identity ok")
```
## Under the hood

Reads the host operating-system and build APIs directly, with no external dependency.
