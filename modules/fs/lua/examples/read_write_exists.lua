-- writes, reads, and probes existence on a path under the repository tree
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
