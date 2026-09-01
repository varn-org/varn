-- readFile pulls a regular file into memory but refuses a non-regular path like a directory so an endless or unreadable stream can never loop forever
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set")

async.run(function()
    local path = dir .. "/readall.txt"
    fs.writeFile(path, "payload-123"):await()

    local content, err = fs.readFile(path):await()
    assert(not err, err)
    assert(content == "payload-123", "a regular file should round-trip through readFile")

    local _, dirErr = fs.readFile(dir):await()
    assert(dirErr, "reading a directory should reject rather than hang or misread")
    assert(dirErr:find("regular file"), "the rejection should name the non-regular cause")

    os.remove(path)
    print("fs readall ok")
end)
