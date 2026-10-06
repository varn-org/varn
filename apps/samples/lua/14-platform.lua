-- What the host is, and which engine is running the script.
local platform = require("platform")

print("os:", platform.os())
print("arch:", platform.arch())
print("cpus:", platform.cpuCount())
print("pointer size:", platform.pointerSize())
print("endianness:", platform.endianness())

local v = platform.version
print("varn:", v.string, string.format("(major %d, minor %d, patch %d)", v.major, v.minor, v.patch))
