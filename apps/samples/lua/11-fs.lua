-- Reads and writes resolve through promises, so the work runs inside a coroutine.
local async = require("async")
local fs = require("fs")
local platform = require("platform")

async.run(function()
    -- A sandboxed app has no writable working directory, so the host names the one place a sample may write.
    local dir = assert(SAMPLE_DIR or os.getenv("VARN_SAMPLE_DIR"), "set VARN_SAMPLE_DIR to a writable directory")
    local path = dir .. "/sample.txt"

    fs.writeFile(path, "written by varn on " .. platform.os()):await()
    print("wrote:", path)

    print("read back:", fs.readFile(path):await())

    -- Exists is answered synchronously, unlike the calls that touch the disk.
    print("exists:", fs.exists(path))

    local info = fs.stat(path):await()
    print("size:", info.size, "is file:", info.isFile)

    fs.removeRecursive(path):await()
    print("still there:", fs.exists(path))
end)
