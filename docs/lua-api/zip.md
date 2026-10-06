# 🗜️ zip

Create, extract, and list ZIP archives. All three return promises.

- `zip.create(archivePath, entries)` — The argument `entries` is an array of `{ file = "...", entry = "..." }`.
- `zip.extract(archivePath, destDir)` — Extracts under `destDir`. Every entry is checked before anything is written: its name must stay inside the destination (zip slip), and the archive may hold at most 100,000 entries whose declared sizes add up to at most 2 GiB (zip bomb). An entry that reads past the size it declares fails the extraction. The entries are written into a new folder beside `destDir` and then moved into place, as one rename when `destDir` does not exist yet and entry by entry when it does, so a refused archive or a failed entry leaves `destDir` as it was. An entry that would replace a symbolic link, or turn a folder into a file or a file into a folder, refuses the extraction before anything moves. Every message names the entry, such as `The entry "../escaped.txt" would leave the destination folder.`.
- `zip.list(archivePath)` → Resolves to an array of `{ name, size, compressedSize, unsafe }`, one per entry in the order of the archive. The field `size` is the uncompressed byte count, `compressedSize` the stored one, and `unsafe` is true for a name `extract` refuses. Listing never refuses an archive for its names, so an app can show what it would not extract.

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
-- Writes a tiny archive and lists its entries with their sizes.
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

    for _, entry in ipairs(entries) do
        print(entry.name, entry.size, entry.compressedSize, entry.unsafe)
    end

    os.remove(archive)
    os.remove(src)
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

    local names = {}
    for i, entry in ipairs(entries) do
        names[i] = entry.name
    end
    print("zip multi-entry create ok: " .. table.concat(names, ", "))
end)
```
### Refused archive

```lua
-- Shows that a refused archive names its entry and writes nothing.
local async = require("async")
local fs = require("fs")
local zip = require("zip")

async.run(function()
    local archive = "build/_refused.zip"
    local outDir = "build/_refused_out"

    -- Lists the entries first so an app can show which ones it refuses.
    local entries, listErr = zip.list(archive):await()
    if listErr then
        print("Cannot list: " .. listErr)
        return
    end

    for _, entry in ipairs(entries) do
        if entry.unsafe then
            print("Unsafe entry: " .. entry.name)
        end
    end

    local _, extractErr = zip.extract(archive, outDir):await()
    print(extractErr, fs.exists(outDir))
end)
```

## Under the hood

Archives are handled by the libzip C++ library, with zlib for compression.
