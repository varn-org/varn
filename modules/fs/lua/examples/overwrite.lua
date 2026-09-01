-- writes a file twice and reads back the replaced content
local async = require("async")
local fs = require("fs")

async.run(function()
    local tmp = "build/_overwrite.txt"

    fs.writeFile(tmp, "first content"):await()
    local first = fs.readFile(tmp):await()
    print("after first write:", first)

    fs.writeFile(tmp, "second"):await()
    local second = fs.readFile(tmp):await()
    print("after overwrite:", second)
    assert(second == "second", "overwrite should replace the content")

    os.remove(tmp)
    print("fs overwrite ok")
end)
