-- writes a temp file, reads it back, and checks existence around a remove
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), "VARN_TEST_DIR is not set; run tests with: python3 varn.py test")

async.run(function()
    local path = dir .. "/fs_test.txt"
    local payload = "varn-fs-test\n"

    fs.writeFile(path, payload):await()
    assert(fs.exists(path), "file should exist after write")

    local content, err = fs.readFile(path):await()
    assert(not err, err)
    assert(content == payload, "round-trip content mismatch")

    os.remove(path)
    assert(not fs.exists(path), "file should be gone after remove")

    print("fs ok")
end)
