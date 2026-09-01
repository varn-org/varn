-- builds example filenames for a short logical library name
local p = require("platform")

local name = "z"
local fn = p.libraryFilename(name)
print("libraryFilename('z') =", fn)

local rel = p.getLibraryPathByName(name, "vendor/libs")
print("getLibraryPathByName('z', 'vendor/libs') =", rel)

assert(fn:match("%."), "expected extension in filename")
print("platform library path helpers ok")
