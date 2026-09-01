# 📁 fs

Filesystem access — read and write whole files, stream large ones through handles, inspect and list
paths, and create scratch directories. Reads and writes run off the event loop and return promises.

```lua
local async = require("async")
local fs = require("fs")

async.run(function()
    fs.writeFile("build/_hello.txt", "hello-fs\n"):await()
    print(fs.readFile("build/_hello.txt"):await())
end)
```

## Capabilities

| Function | What it does |
|---|---|
| `fs.readFile(path)` | Promise resolving to the whole file contents (binary-safe), with no artificial size cap. |
| `fs.writeFile(path, content)` | Promise resolving to `"ok"`; replaces any existing content. |
| `fs.exists(path)` | Boolean, checked synchronously. |
| `fs.open(path, mode)` | Promise resolving to a streaming file handle. `mode` is one of `r`, `w`, `a`, `r+`, `w+`, `a+`. |
| `handle:read(maxBytes)` | Promise resolving to up to `maxBytes` bytes (binary-safe), or `""` at end of file. |
| `handle:write(data)` | Promise resolving to `"ok"`. |
| `handle:close()` | Promise resolving to `"ok"`. |
| `fs.stat(path)` | Promise resolving to `{size, mtime, isDir, isFile, isSymlink}`, or rejecting if the path is missing. |
| `fs.readdir(path)` | Promise resolving to an array of entry names (just the names, not full paths). |
| `fs.rename(from, to)` | Promise resolving to `"ok"`. |
| `fs.copy(from, to)` | Promise resolving to `"ok"`; overwrites the destination if it already exists. |
| `fs.append(path, data)` | Promise resolving to `"ok"`; binary append, creating parent directories as needed. |
| `fs.mkdir(path)` | Promise resolving to `"ok"`; creates the directory and any missing parents. |
| `fs.removeRecursive(path)` | Promise resolving to `"ok"`; removes a file or a directory tree. |
| `fs.mkdtemp(prefix)` | Promise resolving to the path of a freshly created unique temporary directory whose name starts with `prefix`. |

## Reference, examples, and tests

- Full reference: [docs/lua-api/fs.md](../../docs/lua-api/fs.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
