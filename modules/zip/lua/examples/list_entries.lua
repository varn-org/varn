-- writes a tiny archive and lists its member names
local async = require("async")
local zip = require("zip")

async.run(function()
    local src = "build/_list_fixture.txt"
    local archive = "build/_list_test.zip"

    local f = assert(io.open(src, "w"), "cannot write fixture")
    f:write("list-test\n")
    f:close()

    local _, createErr = zip.create(archive, {
        { file = src, entry = "a/one.txt" },
        { file = src, entry = "b/two.txt" },
    }):await()
    assert(not createErr, createErr)

    local entries, listErr = zip.list(archive):await()
    assert(not listErr, listErr)

    local names = {}
    for i = 1, #entries do
        names[entries[i]] = true
    end
    assert(names["a/one.txt"] and names["b/two.txt"], "expected entry names")

    os.remove(archive)
    os.remove(src)

    print("zip.list ok: " .. table.concat(entries, ", "))
end)
