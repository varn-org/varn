-- Creates an archive, lists it, extracts it, and verifies the entry round-trips.
local async = require("async")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set. Run the tests with "python3 varn.py test".')

async.run(function()
    local zip = require("zip")

    local src = dir .. "/zip_src.txt"
    local archive = dir .. "/zip_test.zip"
    local outDir = dir .. "/zip_out"

    local f = assert(io.open(src, "w"), "Cannot create the fixture")
    f:write("zip-test\n")
    f:close()

    local _, createErr = zip.create(archive, { { file = src, entry = "inside/data.txt" } }):await()
    assert(not createErr, createErr)

    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)
    assert(type(entries) == "table" and #entries == 1 and entries[1].name == "inside/data.txt", "List entry")

    local _, extractErr = zip.extract(archive, outDir):await()
    assert(not extractErr, extractErr)

    local rf = assert(io.open(outDir .. "/inside/data.txt", "r"), "Extracted file missing")
    local body = rf:read("*a")
    rf:close()
    assert(body == "zip-test\n", "Extracted content mismatch")

    print("The \"zip\" tests passed.")
end)
