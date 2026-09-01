-- prints the pointer size and byte order of the host build
local p = require("platform")

print("pointerSize()", p.pointerSize())
print("endianness() ", p.endianness())

assert(p.pointerSize() == 4 or p.pointerSize() == 8, "pointerSize")
assert(p.endianness() == "little" or p.endianness() == "big", "endianness")
print("platform byte layout ok")
