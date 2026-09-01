-- prints the shared library prefix and suffix pieces for the host
local p = require("platform")

local prefix = p.libPrefix()
local suffix = p.shlibSuffix()
print("libPrefix()  ", prefix)
print("shlibSuffix()", suffix)

assert(type(prefix) == "string", "libPrefix")
assert(type(suffix) == "string" and #suffix > 0, "shlibSuffix")
print("platform lib naming ok")
