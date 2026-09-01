-- exercises the fs API against hostile paths and content covering traversal (CWE-22), NUL and control bytes (CWE-626, CWE-74), empty path (CWE-20), missing files (CWE-20), large content (CWE-400), and binary content (CWE-626)
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set; run tests with: python3 varn.py test")
local leaf = dir:gsub("[/\\]+$", ""):match("[^/\\]+$")

async.run(function()
    -- FS-001 reads a `../` path that resolves back inside the scratch dir
    local base = dir .. "/fs_sec_base.txt"
    local payload = "fs-sec-traversal"
    fs.writeFile(base, payload):await()
    local viaTraversal, terr = fs.readFile(dir .. "/../" .. leaf .. "/fs_sec_base.txt"):await()
    assert(not terr, terr)
    assert(viaTraversal == payload, "traversal-resolved read should match the original")
    os.remove(base)

    -- FS-007 reads an escaping `../` path that points nowhere
    local escaped, eerr = fs.readFile(dir .. "/../../../fs_sec_nope_zzz.txt"):await()
    assert(escaped == nil, "escaping traversal read should yield nil content")
    assert(eerr, "escaping traversal read should return an error")

    -- FS-057 reads an absent path
    local missing, merr = fs.readFile(dir .. "/fs_sec_missing_zzz.txt"):await()
    assert(missing == nil, "missing read should yield nil content")
    assert(merr, "missing read should return an error")

    -- FS-008, FS-161 reads a path with an embedded NUL byte
    local nulOk, nerr = pcall(function() return fs.readFile(dir .. "/fs_sec\0.txt") end)
    assert(not nulOk, "NUL path should be rejected")
    assert(tostring(nerr):find("null byte"), "NUL path error should name the null byte")

    -- FS-014 reads a path containing a newline control byte
    local ctrlRead, cerr = fs.readFile(dir .. "/fs_sec\n_ctrl.txt"):await()
    assert(ctrlRead == nil, "control-byte path read should yield nil content")
    assert(cerr, "control-byte path read should return an error")

    -- FS-015 reads an empty path string
    local emptyRead, perr = fs.readFile(""):await()
    assert(emptyRead == nil, "empty path read should yield nil content")
    assert(perr, "empty path read should return an error")

    -- FS-038 reads a multi-kilobyte file in full
    local capPath = dir .. "/fs_sec_cap.txt"
    local block = string.rep("cap-", 4096)
    fs.writeFile(capPath, block):await()
    local capBack, kerr = fs.readFile(capPath):await()
    assert(not kerr, kerr)
    assert(#capBack == #block, "sub-cap read should return the full content")
    os.remove(capPath)

    -- FS-050, FS-162 writes and reads back binary content with NUL bytes
    local binPath = dir .. "/fs_sec_bin.txt"
    local binary = "x\0y\0z\0"
    fs.writeFile(binPath, binary):await()
    local binBack = fs.readFile(binPath):await()
    assert(binBack == binary, "binary content should round-trip without truncation")
    assert(#binBack == 6, "binary content length should be preserved")
    os.remove(binPath)

    print("fs security ok")
end)
