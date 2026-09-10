# 🗜️ zip

Create, extract, and list ZIP archives. All three return promises.

- `zip.create(archivePath, entries)` — `entries` is an array of `{ file = "...", entry = "..." }`.
- `zip.extract(archivePath, destDir)` — extracts under `destDir`, rejecting unsafe entry paths (zip slip) and bounding total size and entry count (zip bomb).
- `zip.list(archivePath)` → resolves to an array of entry names.

## Examples

### Create and extract round trip

```lua
-- Creates a small archive and extracts it, verifying the entry round-trips.
local async = require("async")
local fs = require("fs")
local zip = require("zip")

async.run(function()
    local src = "build/_fixture_hello.txt"
    local archive = "build/_test_roundtrip.zip"
    local outDir = "build/_extract_out"

    local f = assert(io.open(src, "w"), "cannot write fixture")
    f:write("zip-roundtrip\n")
    f:close()

    fs.removeRecursive(outDir):await()
    fs.mkdir(outDir):await()

    local _, createErr = zip.create(archive, { { file = src, entry = "inside/hello.txt" } }):await()
    assert(not createErr, createErr)

    local _, extractErr = zip.extract(archive, outDir):await()
    assert(not extractErr, extractErr)

    local rf = assert(io.open(outDir .. "/inside/hello.txt", "r"), "extracted file missing")
    local body = rf:read("*a")
    rf:close()
    assert(body == "zip-roundtrip\n", "content mismatch")

    fs.removeRecursive(outDir):await()
    os.remove(archive)
    os.remove(src)

    print("zip create/extract roundtrip ok")
end)
```

### Listing entries

```lua
-- Writes a tiny archive and lists its member names.
local async = require("async")
local zip = require("zip")

async.run(function()
    local src = "build/_list_fixture.txt"
    local archive = "build/_list_test.zip"

    local f = assert(io.open(src, "w"), "cannot write fixture")
    f:write("list-test\n")
    f:close()

    local _, createErr = zip.create(archive, {
        { file = src, entry = "a/one.txt" },
        { file = src, entry = "b/two.txt" },
    }):await()
    assert(not createErr, createErr)

    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)

    local names = {}
    for i = 1, #entries do
        names[entries[i]] = true
    end
    assert(names["a/one.txt"] and names["b/two.txt"], "expected entry names")

    os.remove(archive)
    os.remove(src)

    print("zip.list ok: " .. table.concat(entries, ", "))
end)
```

### Creating a multi-entry archive

```lua
-- Creates an archive holding several entries including a nested path, and lists them back.
local async = require("async")
local zip = require("zip")

async.run(function()
    local src = "_multi_fixture.txt"
    local archive = "_multi_test.zip"

    local f = assert(io.open(src, "w"), "cannot write fixture")
    f:write("multi-entry\n")
    f:close()

    local _, createErr = zip.create(archive, {
        { file = src, entry = "readme.txt" },
        { file = src, entry = "docs/guide.txt" },
        { file = src, entry = "docs/api/reference.txt" },
    }):await()
    assert(not createErr, createErr)

    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)

    os.remove(archive)
    os.remove(src)

    print("zip multi-entry create ok: " .. table.concat(entries, ", "))
end)
```
## Under the hood

Archives are handled by the libzip C++ library, with zlib for compression.
