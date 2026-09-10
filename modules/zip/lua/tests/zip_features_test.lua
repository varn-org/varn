-- Exercises `create`/`list`/`extract` round-trips, multiple and nested entries, and reachable caps.
local async = require("async")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set. Run the tests with "python3 varn.py test".')

async.run(function()
    local zip = require("zip")

    local src = dir .. "/zip_feat_src.txt"
    local empty = dir .. "/zip_feat_empty.txt"
    local archive = dir .. "/zip_feat.zip"
    local outDir = dir .. "/zip_feat_out"

    local f = assert(io.open(src, "w"), "Cannot create the fixture")
    f:write("zip-features\n")
    f:close()

    -- Writes an empty source file as a valid regular file.
    local ef = assert(io.open(empty, "w"), "Cannot create the empty fixture")
    ef:close()

    -- Creates an archive with multiple entries including nested paths and an empty member.
    local _, createErr = zip.create(archive, {
        { file = src, entry = "top.txt" },
        { file = src, entry = "nested/deep/inside.txt" },
        { file = empty, entry = "empty.txt" },
    }):await()
    assert(not createErr, createErr)

    -- Lists every entry name.
    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)
    assert(type(entries) == "table" and #entries == 3, "Expected three entries")

    local names = {}
    for i = 1, #entries do
        names[entries[i]] = true
    end
    assert(names["top.txt"], "The top entry is listed")
    assert(names["nested/deep/inside.txt"], "The nested entry is listed")
    assert(names["empty.txt"], "The empty entry is listed")

    -- Extracts the tree and verifies the original contents byte for byte.
    local _, extractErr = zip.extract(archive, outDir):await()
    assert(not extractErr, extractErr)

    local rf = assert(io.open(outDir .. "/nested/deep/inside.txt", "r"), "The nested file is missing")
    local body = rf:read("*a")
    rf:close()
    assert(body == "zip-features\n", "The nested content mismatches")

    local eo = assert(io.open(outDir .. "/empty.txt", "r"), "The empty file is missing")
    assert(eo:read("*a") == "", "The empty member should be empty")
    eo:close()

    -- Rejects an empty entries list synchronously.
    assert(not pcall(zip.create, archive, {}), "An empty entries list is rejected")

    -- Rejects an unsafe entry name at create time.
    local _, unsafeErr = zip.create(archive, { { file = src, entry = "../escape.txt" } }):await()
    assert(unsafeErr, "An unsafe create entry is rejected")

    -- Returns an error when listing a missing archive.
    local _, missingErr = zip.list(dir .. "/zip_feat_missing.zip"):await()
    assert(missingErr, "Listing a missing archive errors")

    print("The \"zip\" features tests passed.")
end)
