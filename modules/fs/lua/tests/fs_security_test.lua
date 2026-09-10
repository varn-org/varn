-- Exercises the `fs` API against hostile paths and content covering traversal (CWE-22), NUL and control bytes (CWE-626, CWE-74), empty path (CWE-20), missing files (CWE-20), large content (CWE-400), and binary content (CWE-626).
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')
local leaf = dir:gsub("[/\\]+$", ""):match("[^/\\]+$")

async.run(function()
    -- FS-001: Reads a `../` path that resolves back inside the scratch dir.
    local base = dir .. "/fs_sec_base.txt"
    local payload = "fs-sec-traversal"
    fs.writeFile(base, payload):await()
    local viaTraversal, terr = fs.readFile(dir .. "/../" .. leaf .. "/fs_sec_base.txt"):await()
    assert(not terr, terr)
    assert(viaTraversal == payload, "A traversal-resolved read should match the original")
    os.remove(base)

    -- FS-007: Reads an escaping `../` path that points nowhere.
    local escaped, eerr = fs.readFile(dir .. "/../../../fs_sec_nope_zzz.txt"):await()
    assert(escaped == nil, "An escaping traversal read should yield nil content")
    assert(eerr, "An escaping traversal read should return an error")

    -- FS-057: Reads an absent path.
    local missing, merr = fs.readFile(dir .. "/fs_sec_missing_zzz.txt"):await()
    assert(missing == nil, "A missing read should yield nil content")
    assert(merr, "A missing read should return an error")

    -- FS-008, FS-161: Reads a path with an embedded NUL byte.
    local nulOk, nerr = pcall(function() return fs.readFile(dir .. "/fs_sec\0.txt") end)
    assert(not nulOk, "A NUL path should be rejected")
    assert(tostring(nerr):find("null byte"), "The NUL path error should name the null byte")

    -- FS-014: Reads a path containing a newline control byte.
    local ctrlRead, cerr = fs.readFile(dir .. "/fs_sec\n_ctrl.txt"):await()
    assert(ctrlRead == nil, "A control-byte path read should yield nil content")
    assert(cerr, "A control-byte path read should return an error")

    -- FS-015: Reads an empty path string.
    local emptyRead, perr = fs.readFile(""):await()
    assert(emptyRead == nil, "An empty path read should yield nil content")
    assert(perr, "An empty path read should return an error")

    -- FS-038: Reads a multi-kilobyte file in full.
    local capPath = dir .. "/fs_sec_cap.txt"
    local block = string.rep("cap-", 4096)
    fs.writeFile(capPath, block):await()
    local capBack, kerr = fs.readFile(capPath):await()
    assert(not kerr, kerr)
    assert(#capBack == #block, "A sub-cap read should return the full content")
    os.remove(capPath)

    -- FS-050, FS-162: Writes and reads back binary content with NUL bytes.
    local binPath = dir .. "/fs_sec_bin.txt"
    local binary = "x\0y\0z\0"
    fs.writeFile(binPath, binary):await()
    local binBack = fs.readFile(binPath):await()
    assert(binBack == binary, "Binary content should round-trip without truncation")
    assert(#binBack == 6, "Binary content length should be preserved")
    os.remove(binPath)

    print("The \"fs\" security tests passed.")
end)
