-- Exercises read and write round-trips, existence checks, overwrite, binary safety, and a missing-file error.
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')

async.run(function()
    local path = dir .. "/fs_features.txt"
    local payload = "varn-fs-features\n"

    -- Writes a file and reads back the same content.
    fs.writeFile(path, payload):await()
    assert(fs.exists(path), "File should exist after write")
    local content, err = fs.readFile(path):await()
    assert(not err, err)
    assert(content == payload, "Round-trip content mismatch")

    -- Writes the path again and reads back the new content.
    local replacement = "second-write"
    fs.writeFile(path, replacement):await()
    local after, oerr = fs.readFile(path):await()
    assert(not oerr, oerr)
    assert(after == replacement, "Overwrite should replace content")

    -- Writes and reads back several kilobytes of content.
    local big = string.rep("varn-block-", 1000)
    fs.writeFile(path, big):await()
    local bigBack, berr = fs.readFile(path):await()
    assert(not berr, berr)
    assert(bigBack == big, "Large content round-trip mismatch")
    assert(#bigBack == #big, "Large content length mismatch")

    -- Reads the file and checks it returns content.
    local normal, nerr = fs.readFile(path):await()
    assert(not nerr, nerr)
    assert(#normal > 0, "A normal read should return content")

    -- Writes and reads back binary content with embedded NUL bytes.
    local binary = "a\0b\0c"
    fs.writeFile(path, binary):await()
    local binBack = fs.readFile(path):await()
    assert(binBack == binary, "Binary round-trip mismatch")
    assert(#binBack == 5, "Binary length should be preserved")

    -- Reads a removed path and checks for an error.
    os.remove(path)
    assert(not fs.exists(path), "File should be gone after remove")
    local missing, merr = fs.readFile(path):await()
    assert(missing == nil, "A missing read should yield nil content")
    assert(merr, "A missing read should return an error")

    -- Checks `exists` for an absent path and a written path.
    assert(not fs.exists(path), 'The call "exists" should be false before write')
    fs.writeFile(path, "x"):await()
    assert(fs.exists(path), 'The call "exists" should be true after write')
    os.remove(path)

    print("The \"fs\" feature tests passed.")
end)
