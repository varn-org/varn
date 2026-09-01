# 📁 fs

Filesystem access. Reads and writes run off the main loop and return promises.

- `fs.readFile(path)` → promise resolving to the file contents (binary-safe). Reads the whole file with no artificial size cap, like Node.
- `fs.writeFile(path, content)` → promise resolving to `"ok"`.
- `fs.exists(path)` → boolean, checked synchronously.
- `fs.mkdir(path)` → promise resolving to `"ok"`; creates the directory and any missing parents.
- `fs.removeRecursive(path)` → promise resolving to `"ok"`; removes a file or a whole directory tree.
- `fs.open(path, mode)` → promise resolving to a streaming file handle. `mode` is one of `r`, `w`, `a`, `r+`, `w+`, `a+`. Use this instead of `readFile` for large files so the whole content never sits in memory at once:
  - `handle:read(maxBytes)` → promise resolving to up to `maxBytes` bytes (binary-safe), or an empty string at end of file.
  - `handle:write(data)` → promise resolving to `"ok"`.
  - `handle:close()` → promise resolving to `"ok"`.
  Await each handle call before issuing the next, the same way you would with a Node file handle.
- `fs.stat(path)` → promise resolving to a table `{size, mtime, isDir, isFile, isSymlink}`, or rejecting if the path is missing. `mtime` is epoch seconds; `size` is the byte count for files. A symlink is reported as a symlink while `isDir`/`isFile` follow the link to its target.
- `fs.readdir(path)` → promise resolving to an array of entry names (just the names, not full paths).
- `fs.rename(from, to)` → promise resolving to `"ok"`.
- `fs.copy(from, to)` → promise resolving to `"ok"`. Overwrites the destination if it already exists.
- `fs.append(path, data)` → promise resolving to `"ok"`. Opens the file in binary append mode, creating parent directories as needed.
- `fs.mkdtemp(prefix)` → promise resolving to the path of a freshly created unique temporary directory whose name starts with `prefix`.

Paths are not sandboxed; confine untrusted input before passing it here. A path containing a null byte is rejected rather than silently truncated.

## Examples

### `read_write_exists.lua`

```lua
-- reads writes and probes existence on a path under the repository tree
local async = require("async")
local fs = require("fs")

local marker = "modules/fs/lua/examples/read_write_exists.lua"

async.spawn(function()
    assert(fs.exists(marker), "run from repo root (missing " .. marker .. ")")

    local tmp = "build/_roundtrip.txt"
    fs.writeFile(tmp, "hello-fs\n"):await()
    assert(fs.exists(tmp), "tmp should exist")

    local content, err = fs.readFile(tmp):await()
    assert(not err, err)
    assert(content == "hello-fs\n", "content mismatch")

    os.remove(tmp)
    assert(not fs.exists(tmp), "tmp should be removed")

    print("fs read/write/exists ok")
end)
```

### `stat_readdir.lua`

```lua
-- demonstrates stat readdir append copy rename and mkdtemp under a temporary directory
local async = require("async")
local fs = require("fs")

async.run(function()
    local base = fs.mkdtemp("build/_fs_extra_"):await()

    local file = base .. "/note.txt"
    fs.writeFile(file, "hello"):await()

    local info = fs.stat(file):await()
    print("size:", info.size, "isFile:", info.isFile, "mtime:", info.mtime)

    fs.append(file, " world"):await()
    print("after append:", fs.readFile(file):await())

    fs.copy(file, base .. "/copy.txt"):await()
    fs.rename(base .. "/copy.txt", base .. "/renamed.txt"):await()

    local names = fs.readdir(base):await()
    table.sort(names)
    print("entries:", table.concat(names, ", "))

    fs.removeRecursive(base):await()
    print("fs extra example ok")
end)
```

## Under the hood

Built on the C++ standard library filesystem, with reads and writes running on a background I/O thread pool so they never block the event loop.
