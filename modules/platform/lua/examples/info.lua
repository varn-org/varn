-- prints host identifiers, system data, and shared library naming hints
local p = require("platform")

print("os()         ", p.os())
print("arch()       ", p.arch())
print("hostVersion()", p.hostVersion())
print("cpuCount()   ", p.cpuCount())
print("pointerSize()", p.pointerSize())
print("endianness() ", p.endianness())
print("libPrefix()  ", p.libPrefix())
print("shlibSuffix()", p.shlibSuffix())

assert(type(p.os()) == "string" and #p.os() > 0, "os")
assert(type(p.arch()) == "string", "arch")
assert(p.cpuCount() >= 1, "cpuCount")
assert(p.pointerSize() == 4 or p.pointerSize() == 8, "pointerSize")
assert(p.endianness() == "little" or p.endianness() == "big", "endianness")

print("platform info ok")
