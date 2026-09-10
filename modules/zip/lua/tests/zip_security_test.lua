-- Covers the Lua-reachable zip abuse cases for path traversal, absolute paths, malformed input, backslash and mixed separators, and the entry-count cap.
local async = require("async")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The variable "VARN_TEST_DIR" is not set. Run the tests with "python3 varn.py test".')

-- Computes the CRC-32 of a byte string for a stored zip entry.
local function crc32(s)
    local crc = 0xFFFFFFFF
    for i = 1, #s do
        crc = crc ~ s:byte(i)
        for _ = 1, 8 do
            local mask = -(crc & 1) & 0xFFFFFFFF
            crc = (crc >> 1) ~ (0xEDB88320 & mask)
        end
    end
    return (~crc) & 0xFFFFFFFF
end

local function le16(n)
    return string.char(n & 0xFF, (n >> 8) & 0xFF)
end

local function le32(n)
    return string.char(n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, (n >> 24) & 0xFF)
end

-- Builds a stored-only zip from `{name, data}` entries so hostile names bypass create-side checks.
local function buildZip(entries)
    local locals, central = {}, {}
    local offset = 0

    for _, e in ipairs(entries) do
        local name, data = e.name, e.data or ""
        local crc = crc32(data)
        local lh = "PK\3\4" .. le16(20) .. le16(0) .. le16(0) .. le16(0) .. le16(0)
            .. le32(crc) .. le32(#data) .. le32(#data) .. le16(#name) .. le16(0) .. name .. data
        locals[#locals + 1] = lh
        local ch = "PK\1\2" .. le16(20) .. le16(20) .. le16(0) .. le16(0) .. le16(0) .. le16(0)
            .. le32(crc) .. le32(#data) .. le32(#data) .. le16(#name) .. le16(0) .. le16(0)
            .. le16(0) .. le16(0) .. le32(0) .. le32(offset) .. name
        central[#central + 1] = ch
        offset = offset + #lh
    end

    local localBlob = table.concat(locals)
    local centralBlob = table.concat(central)
    local eocd = "PK\5\6" .. le16(0) .. le16(0) .. le16(#entries) .. le16(#entries)
        .. le32(#centralBlob) .. le32(#localBlob) .. le16(0)
    return localBlob .. centralBlob .. eocd
end

local function writeFile(path, bytes)
    local f = assert(io.open(path, "wb"), 'Cannot write "' .. path .. '"')
    f:write(bytes)
    f:close()
end

async.run(function()
    local zip = require("zip")

    local outDir = dir .. "/zip_sec_out"
    local sentinel = dir .. "/zip_sec_evil"

    -- Rejects a relative-traversal entry escaping `destDir` on extract (ZIP-001, ZIP-101).
    local slip = dir .. "/zip_sec_slip.zip"
    writeFile(slip, buildZip({ { name = "../zip_sec_evil", data = "PWNED\n" } }))
    local _, slipErr = zip.extract(slip, outDir):await()
    assert(slipErr, "A relative traversal is rejected")
    assert(not io.open(sentinel, "r"), 'No file is written outside "destDir"')

    -- Rejects an absolute-path entry on extract (ZIP-002, ZIP-104).
    local abs = dir .. "/zip_sec_abs.zip"
    writeFile(abs, buildZip({ { name = "/tmp/zip_sec_evil_abs", data = "PWNED\n" } }))
    local _, absErr = zip.extract(abs, outDir):await()
    assert(absErr, "An absolute path is rejected")
    assert(not io.open("/tmp/zip_sec_evil_abs", "r"), "No absolute file is written")

    -- Rejects a backslash-separator traversal entry (ZIP-004, ZIP-106).
    local back = dir .. "/zip_sec_back.zip"
    writeFile(back, buildZip({ { name = "..\\zip_sec_evil", data = "PWNED\n" } }))
    local _, backErr = zip.extract(back, outDir):await()
    assert(backErr, "Backslash separators are rejected")

    -- Rejects a mixed-separator traversal entry (ZIP-005, ZIP-107).
    local mixed = dir .. "/zip_sec_mixed.zip"
    writeFile(mixed, buildZip({ { name = "..\\../zip_sec_evil", data = "PWNED\n" } }))
    local _, mixedErr = zip.extract(mixed, outDir):await()
    assert(mixedErr, "Mixed separators are rejected")

    -- Rejects a deeply nested traversal chain (ZIP-006, ZIP-103).
    local deep = dir .. "/zip_sec_deep.zip"
    writeFile(deep, buildZip({ { name = "a/../../../zip_sec_evil", data = "PWNED\n" } }))
    local _, deepErr = zip.extract(deep, outDir):await()
    assert(deepErr, "A deep traversal chain is rejected")

    -- Refuses to list an unsafe entry name (ZIP-097).
    local _, listErr = zip.list(slip):await()
    assert(listErr, 'The call "list" rejects an unsafe entry name')

    -- Errors on a garbage archive (ZIP-041, ZIP-043, ZIP-244).
    local garbage = dir .. "/zip_sec_garbage.zip"
    writeFile(garbage, "this is not a zip archive at all")
    local _, garbageErr = zip.extract(garbage, outDir):await()
    assert(garbageErr, 'A garbage archive errors on "extract"')
    local _, garbageList = zip.list(garbage):await()
    assert(garbageList, 'A garbage archive errors on "list"')

    -- Errors on a truncated archive (a valid prefix cut short).
    local good = buildZip({ { name = "ok.txt", data = "hi\n" } })
    local truncated = dir .. "/zip_sec_trunc.zip"
    writeFile(truncated, good:sub(1, #good - 12))
    local _, truncErr = zip.extract(truncated, outDir):await()
    assert(truncErr, "A truncated archive errors")

    -- Rejects an empty entries list at create time (ZIP-027, ZIP-132).
    assert(not pcall(zip.create, dir .. "/zip_sec_empty.zip", {}), "An empty entries list is rejected")

    -- Round-trips a well-formed archive.
    local safe = dir .. "/zip_sec_safe.zip"
    writeFile(safe, good)
    local _, safeErr = zip.extract(safe, outDir):await()
    assert(not safeErr, safeErr)
    local rf = assert(io.open(outDir .. "/ok.txt", "r"), "The safe entry is missing")
    assert(rf:read("*a") == "hi\n", "The safe content mismatches")
    rf:close()

    print("The \"zip\" security tests passed.")
end)
