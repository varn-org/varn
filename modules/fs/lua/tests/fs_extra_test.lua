-- Exercises `stat`, `readdir`, `append`, `copy`, `rename`, and `mkdtemp` against a scratch directory.
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')

async.run(function()
    local path = dir .. "/fs_extra.txt"
    local payload = "varn-fs-extra\n"

    -- Writes a file and stats it for size, kind, and mtime.
    fs.writeFile(path, payload):await()
    local info, serr = fs.stat(path):await()
    assert(not serr, serr)
    assert(info.size == #payload, 'The "stat" size mismatches')
    assert(info.isFile, 'The call "stat" should report a file')
    assert(not info.isDir, 'The call "stat" should not report a directory')
    assert(info.mtime > 0, 'The "stat" mtime should be positive')

    -- Stats a missing path.
    local _, merr = fs.stat(dir .. "/fs_extra_missing.txt"):await()
    assert(merr, 'The call "stat" on a missing path should reject')

    -- Creates a directory and lists its entries by name.
    local sub = dir .. "/fs_extra_dir"
    fs.mkdir(sub):await()
    fs.writeFile(sub .. "/one.txt", "1"):await()
    fs.writeFile(sub .. "/two.txt", "2"):await()
    local names, derr = fs.readdir(sub):await()
    assert(not derr, derr)
    local seen = {}
    for _, name in ipairs(names) do
        seen[name] = true
    end
    assert(seen["one.txt"], 'The call "readdir" should list "one.txt"')
    assert(seen["two.txt"], 'The call "readdir" should list "two.txt"')

    -- Appends to the file and reads back the combined content.
    fs.append(path, "more"):await()
    local appended, aerr = fs.readFile(path):await()
    assert(not aerr, aerr)
    assert(appended == payload .. "more", "Append round-trip mismatch")

    -- Copies the file and compares the two contents.
    local copyPath = dir .. "/fs_extra_copy.txt"
    fs.copy(path, copyPath):await()
    local copied = fs.readFile(copyPath):await()
    assert(copied == appended, "Copy content mismatch")

    -- Renames the file and checks the source and destination paths.
    local renamed = dir .. "/fs_extra_renamed.txt"
    fs.rename(copyPath, renamed):await()
    assert(not fs.exists(copyPath), 'The call "rename" should remove the source')
    assert(fs.exists(renamed), 'The call "rename" should create the destination')

    -- Creates a unique temporary directory under the given prefix.
    local tempDir, terr = fs.mkdtemp(dir .. "/fs_extra_tmp_"):await()
    assert(not terr, terr)
    assert(fs.exists(tempDir), 'The "mkdtemp" directory should exist')
    local tstat = fs.stat(tempDir):await()
    assert(tstat.isDir, 'The call "mkdtemp" should create a directory')

    os.remove(path)
    os.remove(renamed)
    print("The \"fs\" extra tests passed.")
end)
