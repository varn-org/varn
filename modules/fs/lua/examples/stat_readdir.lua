-- Demonstrates stat readdir append copy rename and mkdtemp under a temporary directory.
local async = require("async")
local fs = require("fs")

async.run(function()
    -- Creates a private scratch directory.
    local base = fs.mkdtemp("build/_fs_extra_"):await()
    print("scratch dir:", base)

    local file = base .. "/note.txt"
    fs.writeFile(file, "hello"):await()

    -- Stats the file for size and kind.
    local info = fs.stat(file):await()
    print("size:", info.size, "isFile:", info.isFile, "mtime:", info.mtime)

    -- Appends to the file.
    fs.append(file, " world"):await()
    print("after append:", fs.readFile(file):await())

    -- Copies then renames the file within the scratch directory.
    fs.copy(file, base .. "/copy.txt"):await()
    fs.rename(base .. "/copy.txt", base .. "/renamed.txt"):await()

    -- Lists the directory entries by name.
    local names = fs.readdir(base):await()
    table.sort(names)
    print("entries:", table.concat(names, ", "))

    fs.removeRecursive(base):await()
    print("fs extra example ok")
end)
