-- Writes a temp file, reads it back, and checks existence around a remove.
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')

async.run(function()
    local path = dir .. "/fs_test.txt"
    local payload = "varn-fs-test\n"

    fs.writeFile(path, payload):await()
    assert(fs.exists(path), "File should exist after write")

    local content, err = fs.readFile(path):await()
    assert(not err, err)
    assert(content == payload, "Round-trip content mismatch")

    os.remove(path)
    assert(not fs.exists(path), "File should be gone after remove")

    print("The \"fs\" tests passed.")
end)
