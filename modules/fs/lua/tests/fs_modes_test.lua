-- fs open modes, removeRecursive, and the reject paths that the main fs tests do not exercise
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set")

async.run(function()
    -- a non-positive handle read bound is rejected
    fs.writeFile(dir .. "/modes.txt", "hello"):await()
    local rh = fs.open(dir .. "/modes.txt", "r"):await()
    assert(not pcall(function() return rh:read(0):await() end), "a non-positive read bound should be rejected")
    rh:close():await()

    -- append mode adds to the end of an existing file
    fs.writeFile(dir .. "/app.txt", "AB"):await()
    local ah = fs.open(dir .. "/app.txt", "a"):await()
    ah:write("CD"):await()
    ah:close():await()
    assert(fs.readFile(dir .. "/app.txt"):await() == "ABCD", "append mode should add to the end")

    -- r+ reads and writes in place, w+ truncates then writes
    fs.writeFile(dir .. "/rw.txt", "0123456789"):await()
    local rp = fs.open(dir .. "/rw.txt", "r+"):await()
    rp:write("XY"):await()
    rp:close():await()
    assert(fs.readFile(dir .. "/rw.txt"):await() == "XY23456789", "r+ should overwrite in place without truncating")

    local wp = fs.open(dir .. "/wp.txt", "w+"):await()
    wp:write("fresh"):await()
    wp:close():await()
    assert(fs.readFile(dir .. "/wp.txt"):await() == "fresh", "w+ should write a fresh file")

    -- writeFile and append create missing parent directories
    fs.writeFile(dir .. "/nested/deep/created.txt", "x"):await()
    assert(fs.exists(dir .. "/nested/deep/created.txt"), "writeFile should create parent directories")

    -- removeRecursive deletes a whole tree
    fs.mkdir(dir .. "/tree/sub"):await()
    fs.writeFile(dir .. "/tree/sub/f.txt", "x"):await()
    assert(fs.exists(dir .. "/tree/sub/f.txt"), "the tree should exist before removal")
    fs.removeRecursive(dir .. "/tree"):await()
    assert(not fs.exists(dir .. "/tree"), "removeRecursive should delete the whole tree")

    -- readdir on a missing directory and copy or rename of a missing source are all rejected
    local _, rdErr = fs.readdir(dir .. "/nope"):await()
    assert(rdErr, "readdir on a missing directory should reject")
    local _, cpErr = fs.copy(dir .. "/nope.txt", dir .. "/dst.txt"):await()
    assert(cpErr, "copy of a missing source should reject")
    local _, mvErr = fs.rename(dir .. "/nope.txt", dir .. "/dst2.txt"):await()
    assert(mvErr, "rename of a missing source should reject")

    print("fs modes ok")
end)
