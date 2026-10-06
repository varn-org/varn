-- Exercises `create`/`list`/`extract` round-trips, multiple and nested entries, the listed fields, and extraction into new and existing folders.
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set. Run the tests with "python3 varn.py test".')

async.run(function()
    local zip = require("zip")

    local src = dir .. "/zip_feat_src.txt"
    local empty = dir .. "/zip_feat_empty.txt"
    local archive = dir .. "/zip_feat.zip"
    local outDir = dir .. "/zip_feat_out"

    local f = assert(io.open(src, "wb"), "Cannot create the fixture")
    f:write("zip-features\n")
    f:close()

    -- Writes an empty source file as a valid regular file.
    local ef = assert(io.open(empty, "wb"), "Cannot create the empty fixture")
    ef:close()

    -- Creates an archive with multiple entries including nested paths and an empty member.
    local _, createErr = zip.create(archive, {
        { file = src, entry = "top.txt" },
        { file = src, entry = "nested/deep/inside.txt" },
        { file = empty, entry = "empty.txt" },
    }):await()
    assert(not createErr, createErr)

    -- Lists every entry with its sizes and whether extracting it would be refused.
    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)
    assert(type(entries) == "table" and #entries == 3, "Expected three entries")

    local listed = {}
    for i = 1, #entries do
        listed[entries[i].name] = entries[i]
    end
    assert(listed["top.txt"], "The top entry is listed")
    assert(listed["nested/deep/inside.txt"], "The nested entry is listed")
    assert(listed["empty.txt"], "The empty entry is listed")
    assert(listed["top.txt"].size == #"zip-features\n", "The size is the uncompressed byte count")
    assert(listed["empty.txt"].size == 0, "An empty entry has size zero")
    assert(math.type(listed["top.txt"].compressedSize) == "integer", 'The field "compressedSize" is an integer')
    assert(listed["top.txt"].unsafe == false, "A plain entry is safe")

    -- Extracts the tree and verifies the original contents byte for byte.
    local _, extractErr = zip.extract(archive, outDir):await()
    assert(not extractErr, extractErr)

    local rf = assert(io.open(outDir .. "/nested/deep/inside.txt", "rb"), "The nested file is missing")
    local body = rf:read("*a")
    rf:close()
    assert(body == "zip-features\n", "The nested content mismatches")

    local eo = assert(io.open(outDir .. "/empty.txt", "rb"), "The empty file is missing")
    assert(eo:read("*a") == "", "The empty member should be empty")
    eo:close()

    -- Creates a missing destination and its parents, leaving no staging folder beside it.
    local fresh = dir .. "/zip_feat_parent/fresh"
    local _, freshErr = zip.extract(archive, fresh):await()
    assert(not freshErr, freshErr)
    local ff = assert(io.open(fresh .. "/top.txt", "rb"), "The fresh destination is missing its entry")
    assert(ff:read("*a") == "zip-features\n", "The fresh content mismatches")
    ff:close()
    local siblings = fs.readdir(dir .. "/zip_feat_parent"):await()
    assert(#siblings == 1 and siblings[1] == "fresh", "Only the destination remains beside it")

    -- Merges into an existing destination, replacing its files and keeping the ones the archive does not name.
    local merged = dir .. "/zip_feat_merged"
    fs.mkdir(merged .. "/nested"):await()
    fs.writeFile(merged .. "/keep.txt", "kept"):await()
    fs.writeFile(merged .. "/top.txt", "old"):await()
    local _, mergeErr = zip.extract(archive, merged):await()
    assert(not mergeErr, mergeErr)
    assert(fs.readFile(merged .. "/keep.txt"):await() == "kept", "An unrelated file is kept")
    assert(fs.readFile(merged .. "/top.txt"):await() == "zip-features\n", "A named file is replaced")
    assert(fs.readFile(merged .. "/nested/deep/inside.txt"):await() == "zip-features\n", "An existing folder receives the nested entry")

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
