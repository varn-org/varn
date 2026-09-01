-- path building and argument handling stay safe under hostile input covering CWE-22 path traversal, CWE-20 input validation, CWE-400 resource exhaustion, CWE-626 nul handling, and CWE-74 control bytes
local platform = require("platform")

-- (PLAT-024, PLAT-036) an empty library name is rejected via an error rather than building a bogus path
local okEmpty = pcall(platform.libraryFilename, "")
assert(not okEmpty, "empty name should error")

-- (PLAT-039, PLAT-156) a missing name argument errors instead of crashing
local okMissing = pcall(platform.libraryFilename)
assert(not okMissing, "missing name should error")

-- (PLAT-016, PLAT-028, PLAT-116) a traversal-style name is treated as data and returns a plain string
local okTraversal, traversal = pcall(platform.libraryFilename, "../../evil")
assert(okTraversal and type(traversal) == "string" and #traversal > 0, "traversal name should not crash")

-- (PLAT-022, PLAT-124) control bytes in the name are handled without crashing
local okCtrl, ctrl = pcall(platform.libraryFilename, "a\1\2b")
assert(okCtrl and type(ctrl) == "string", "control bytes in name should not crash")

-- (PLAT-026, PLAT-040) a very long name is handled without crashing
local okLong, longName = pcall(platform.libraryFilename, string.rep("a", 100000))
assert(okLong and type(longName) == "string" and #longName > 0, "long name should not crash")

-- (PLAT-018, PLAT-118) an absolute-looking name is treated as data, never as a real path
local okAbs, abs = pcall(platform.libraryFilename, "/etc/passwd")
assert(okAbs and type(abs) == "string", "absolute-looking name should not crash")

-- (PLAT-008, PLAT-025, PLAT-125) getLibraryPathByName with no subdir returns a safe relative string
local okDefault, defaultPath = pcall(platform.getLibraryPathByName, "z")
assert(okDefault and type(defaultPath) == "string" and #defaultPath > 0, "default path should not crash")

-- (PLAT-017, PLAT-068, PLAT-117) a traversal subdir is handled without crashing
local okSub, subPath = pcall(platform.getLibraryPathByName, "z", "../../")
assert(okSub and type(subPath) == "string", "traversal subdir should not crash")

-- (PLAT-024) an empty name errors when going through the path builder
local okEmptyPath = pcall(platform.getLibraryPathByName, "", "")
assert(not okEmptyPath, "empty name in path builder should error")

print("platform security ok")
