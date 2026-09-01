-- exercises create/list/extract round-trips, multiple and nested entries, and reachable caps
local async = require("async")

local dir = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set; run tests with: python3 varn.py test")

async.run(function()
    local zip = require("zip")

    local src = dir .. "/zip_feat_src.txt"
    local empty = dir .. "/zip_feat_empty.txt"
    local archive = dir .. "/zip_feat.zip"
    local outDir = dir .. "/zip_feat_out"

    local f = assert(io.open(src, "w"), "cannot create fixture")
    f:write("zip-features\n")
    f:close()

    -- writes an empty source file as a valid regular file
    local ef = assert(io.open(empty, "w"), "cannot create empty fixture")
    ef:close()

    -- creates an archive with multiple entries including nested paths and an empty member
    local _, createErr = zip.create(archive, {
        { file = src, entry = "top.txt" },
        { file = src, entry = "nested/deep/inside.txt" },
        { file = empty, entry = "empty.txt" },
    }):await()
    assert(not createErr, createErr)

    -- lists every entry name
    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)
    assert(type(entries) == "table" and #entries == 3, "expected three entries")

    local names = {}
    for i = 1, #entries do
        names[entries[i]] = true
    end
    assert(names["top.txt"], "top entry listed")
    assert(names["nested/deep/inside.txt"], "nested entry listed")
    assert(names["empty.txt"], "empty entry listed")

    -- extracts the tree and verifies the original contents byte for byte
    local _, extractErr = zip.extract(archive, outDir):await()
    assert(not extractErr, extractErr)

    local rf = assert(io.open(outDir .. "/nested/deep/inside.txt", "r"), "nested file missing")
    local body = rf:read("*a")
    rf:close()
    assert(body == "zip-features\n", "nested content mismatch")

    local eo = assert(io.open(outDir .. "/empty.txt", "r"), "empty file missing")
    assert(eo:read("*a") == "", "empty member should be empty")
    eo:close()

    -- rejects an empty entries list synchronously
    assert(not pcall(zip.create, archive, {}), "empty entries list rejected")

    -- rejects an unsafe entry name at create time
    local _, unsafeErr = zip.create(archive, { { file = src, entry = "../escape.txt" } }):await()
    assert(unsafeErr, "unsafe create entry rejected")

    -- returns an error when listing a missing archive
    local _, missingErr = zip.list(dir .. "/zip_feat_missing.zip"):await()
    assert(missingErr, "missing archive list errors")

    print("zip features ok")
end)
