-- An archive names each entry independently of where the file sits on disk.
local async = require("async")
local fs = require("fs")
local zip = require("zip")

async.run(function()
    -- A sandboxed app has no writable working directory, so the host names the one place a sample may write.
    local dir = assert(SAMPLE_DIR or os.getenv("VARN_SAMPLE_DIR"), "set VARN_SAMPLE_DIR to a writable directory")
    local archive = dir .. "/sample.zip"

    fs.writeFile(dir .. "/a.txt", "first file"):await()
    fs.writeFile(dir .. "/b.txt", "second file"):await()

    zip.create(archive, {
        { file = dir .. "/a.txt", entry = "docs/a.txt" },
        { file = dir .. "/b.txt", entry = "docs/b.txt" },
    }):await()
    print("created:", archive)

    for _, name in ipairs(zip.list(archive):await()) do
        print("entry:", name)
    end

    zip.extract(archive, dir .. "/out"):await()
    print("extracted:", fs.readFile(dir .. "/out/docs/a.txt"):await())
end)
