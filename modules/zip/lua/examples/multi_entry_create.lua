-- creates an archive holding several entries including a nested path, and lists them back
local async = require("async")
local zip = require("zip")

async.run(function()
    local src = "_multi_fixture.txt"
    local archive = "_multi_test.zip"

    local f = assert(io.open(src, "w"), "cannot write fixture")
    f:write("multi-entry\n")
    f:close()

    local _, createErr = zip.create(archive, {
        { file = src, entry = "readme.txt" },
        { file = src, entry = "docs/guide.txt" },
        { file = src, entry = "docs/api/reference.txt" },
    }):await()
    assert(not createErr, createErr)

    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)

    os.remove(archive)
    os.remove(src)

    print("zip multi-entry create ok: " .. table.concat(entries, ", "))
end)
