-- Covers the Lua-reachable zip abuse cases for path traversal, absolute paths, malformed input, separators, declared sizes, partial extraction and the destination folder.
local async = require("async")
local fs = require("fs")

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

-- Builds a stored-only zip from `{name, data, size}` entries so hostile names and declared sizes bypass create-side checks.
local function buildZip(entries)
    local locals, central = {}, {}
    local offset = 0

    for _, e in ipairs(entries) do
        local name, data = e.name, e.data or ""
        local size = e.size or #data
        local crc = crc32(data)
        local lh = "PK\3\4" .. le16(20) .. le16(0) .. le16(0) .. le16(0) .. le16(0)
            .. le32(crc) .. le32(#data) .. le32(size) .. le16(#name) .. le16(0) .. name .. data
        locals[#locals + 1] = lh
        local ch = "PK\1\2" .. le16(20) .. le16(20) .. le16(0) .. le16(0) .. le16(0) .. le16(0)
            .. le32(crc) .. le32(#data) .. le32(size) .. le16(#name) .. le16(0) .. le16(0)
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

-- Answers whether a folder holds a staging folder an extraction into `name` left beside it.
local function staged(parent, name)
    for _, entry in ipairs(fs.readdir(parent):await()) do
        if entry:find("." .. name .. ".extracting-", 1, true) == 1 then
            return true
        end
    end
    return false
end

-- Answers whether an extraction into `name` left anything, the destination itself or a staging folder.
local function leftovers(parent, name)
    return fs.exists(parent .. "/" .. name) or staged(parent, name)
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

    -- Leaves nothing behind when an entry after safe ones is refused, and names the entry (ZIP-084).
    local mixedEntries = {
        { name = "first.txt", data = "1" },
        { name = "second.txt", data = "2" },
        { name = "../escaped.txt", data = "PWNED\n" },
        { name = "third.txt", data = "3" },
    }
    local partial = dir .. "/zip_sec_partial.zip"
    writeFile(partial, buildZip(mixedEntries))
    local _, partialErr = zip.extract(partial, dir .. "/zip_sec_partial_out"):await()
    assert(tostring(partialErr):find('The entry "../escaped.txt" would leave the destination folder.', 1, true), tostring(partialErr))
    assert(not leftovers(dir, "zip_sec_partial_out"), "A refused archive writes nothing")
    assert(not io.open(dir .. "/escaped.txt", "r"), "The escaping entry is not written")

    -- Leaves an existing destination exactly as it was when the archive is refused (ZIP-084).
    local kept = dir .. "/zip_sec_kept"
    fs.mkdir(kept):await()
    writeFile(kept .. "/first.txt", "original")
    local _, keptErr = zip.extract(partial, kept):await()
    assert(keptErr, "The archive is refused")
    assert(#fs.readdir(kept):await() == 1 and fs.readFile(kept .. "/first.txt"):await() == "original", "The existing destination is untouched")

    -- Lists an archive holding unsafe names and flags each one instead of refusing it (ZIP-097).
    local listed, listErr = zip.list(partial):await()
    assert(not listErr, listErr)
    assert(#listed == 4, "Every entry is listed")
    for i, entry in ipairs(listed) do
        assert(entry.name == mixedEntries[i].name, "The names keep the order of the archive")
        assert(entry.size == #mixedEntries[i].data and entry.compressedSize == #mixedEntries[i].data, "A stored entry lists its sizes")
        assert(entry.unsafe == (entry.name == "../escaped.txt"), "Only the escaping entry is unsafe")
    end
    assert(zip.list(abs):await()[1].unsafe, "An absolute name is unsafe")

    -- Refuses an archive whose entries declare more than the total limit before writing anything (ZIP-024, ZIP-025).
    local bomb = dir .. "/zip_sec_bomb.zip"
    writeFile(bomb, buildZip({
        { name = "small.txt", data = "ok" },
        { name = "big1.bin", data = "x", size = 0x60000000 },
        { name = "big2.bin", data = "x", size = 0x60000000 },
    }))
    local _, bombErr = zip.extract(bomb, dir .. "/zip_sec_bomb_out"):await()
    assert(tostring(bombErr):find("The entries of the archive declare more than the limit of 2 GiB.", 1, true), tostring(bombErr))
    assert(not leftovers(dir, "zip_sec_bomb_out"), "A bomb writes nothing")

    -- Refuses an entry whose data runs past the size it declares and removes what was already written (ZIP-026, ZIP-185).
    local liar = dir .. "/zip_sec_liar.zip"
    writeFile(liar, buildZip({
        { name = "fine.txt", data = "fine" },
        { name = "liar.txt", data = "longer than declared", size = 4 },
    }))
    local _, liarErr = zip.extract(liar, dir .. "/zip_sec_liar_out"):await()
    assert(tostring(liarErr):find('"liar.txt"', 1, true), tostring(liarErr))
    assert(not leftovers(dir, "zip_sec_liar_out"), "A failed entry leaves nothing behind")

    -- Refuses to turn a folder of the destination into a file and moves nothing in (ZIP-021).
    local conflict = dir .. "/zip_sec_conflict"
    fs.mkdir(conflict .. "/item"):await()
    local itemZip = dir .. "/zip_sec_item.zip"
    writeFile(itemZip, buildZip({ { name = "new.txt", data = "new" }, { name = "item", data = "file" } }))
    local _, conflictErr = zip.extract(itemZip, conflict):await()
    assert(tostring(conflictErr):find('The entry "item" would replace a folder in the destination folder.', 1, true), tostring(conflictErr))
    assert(not fs.exists(conflict .. "/new.txt"), "No entry is moved in when one conflicts")
    assert(not staged(dir, "zip_sec_conflict"), "No staging folder is left behind")

    -- Refuses to write through a symbolic link inside an existing destination (ZIP-013, ZIP-017).
    if package.config:sub(1, 1) == "/" then
        local linked = dir .. "/zip_sec_linked"
        local outside = dir .. "/zip_sec_outside"
        fs.mkdir(linked):await()
        fs.mkdir(outside):await()
        assert(os.execute('ln -s "' .. outside .. '" "' .. linked .. '/link"'), "Cannot create the symbolic link")
        local linkZip = dir .. "/zip_sec_link.zip"
        writeFile(linkZip, buildZip({ { name = "link/evil.txt", data = "PWNED\n" } }))
        local _, linkErr = zip.extract(linkZip, linked):await()
        assert(tostring(linkErr):find('The entry "link" would replace a symbolic link in the destination folder.', 1, true), tostring(linkErr))
        assert(#fs.readdir(outside):await() == 0, "Nothing is written through the link")
    end

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
