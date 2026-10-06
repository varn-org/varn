# 🗜️ zip

Create, extract, and list ZIP archives — backed by libzip with zlib for compression. Every call returns a promise.

```lua
local async = require("async")
local zip = require("zip")

async.run(function()
    zip.create("out.zip", { { file = "hello.txt", entry = "hello.txt" } }):await()
end)
```

## Capabilities

| Function | What it does |
|---|---|
| `zip.create(archivePath, entries)` | Build an archive from `entries`, an array of `{ file = "...", entry = "..." }`. |
| `zip.extract(archivePath, destDir)` | Extract under `destDir` after checking every entry first, rejecting unsafe entry paths (zip slip) and bounding the declared total size and the entry count (zip bomb), through a folder beside `destDir` that moves into place only once every entry was written. |
| `zip.list(archivePath)` | Resolve to an array of `{ name, size, compressedSize, unsafe }`, flagging the entries `extract` would refuse instead of failing. |

## Reference and tests

- Full reference: [docs/lua-api/zip.md](../../docs/lua-api/zip.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
