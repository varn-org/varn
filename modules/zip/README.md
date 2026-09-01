# 🗜️ zip

Create, extract, and list ZIP archives — backed by libzip with zlib for compression. Every call
returns a promise.

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
| `zip.extract(archivePath, destDir)` | Extract under `destDir`, rejecting unsafe entry paths (zip slip) and bounding total size and entry count (zip bomb). |
| `zip.list(archivePath)` | Resolve to an array of the archive's entry names. |

## Reference, examples, and tests

- Full reference: [docs/lua-api/zip.md](../../docs/lua-api/zip.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
