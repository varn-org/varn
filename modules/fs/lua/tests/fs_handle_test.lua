-- Writes chunks through a streaming file handle and reads them back.
local async = require("async")
local fs = require("fs")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')

async.run(function()
    local path = dir .. "/fs_handle_test.bin"

    -- Writes three chunks through one handle.
    local writer, werr = fs.open(path, "w"):await()
    assert(not werr, werr)
    writer:write("abc"):await()
    writer:write("defgh"):await()
    writer:close():await()

    -- Reads back in small reads until end of file yields an empty string.
    local reader, rerr = fs.open(path, "r"):await()
    assert(not rerr, rerr)

    local parts = {}
    while true do
        local chunk, cerr = reader:read(2):await()
        assert(not cerr, cerr)
        if chunk == "" then
            break
        end
        parts[#parts + 1] = chunk
    end
    reader:close():await()
    assert(table.concat(parts) == "abcdefgh", "Chunked round-trip mismatch")

    -- Reads after close.
    local _, aerr = reader:read(1):await()
    assert(aerr, "A read after close should reject")

    -- Writes and reads back binary content with embedded NUL bytes.
    local bin = "x\0y\0z"
    local bw = fs.open(path, "w"):await()
    bw:write(bin):await()
    bw:close():await()

    local br = fs.open(path, "r"):await()
    local back = br:read(64):await()
    br:close():await()
    assert(back == bin, "Binary round-trip mismatch")

    -- Opens with an unknown mode.
    local _, merr = fs.open(path, "zzz"):await()
    assert(merr, "An unknown mode should reject")

    os.remove(path)
    print("The \"fs\" handle tests passed.")
end)
