-- platform host identifiers and system data are present and well-formed
local platform = require("platform")

local osId = platform.os()
assert(type(osId) == "string" and #osId > 0, "os")

local arch = platform.arch()
assert(type(arch) == "string" and #arch > 0, "arch")

assert(type(platform.hostVersion()) == "string", "hostVersion")
assert(platform.cpuCount() >= 1, "cpuCount")

local pointer = platform.pointerSize()
assert(pointer == 4 or pointer == 8, "pointerSize")

local endian = platform.endianness()
assert(endian == "little" or endian == "big", "endianness")

assert(#platform.shlibSuffix() > 0, "shlibSuffix")
assert(type(platform.libPrefix()) == "string", "libPrefix")
assert(#platform.libraryFilename("sqlite3") > 0, "libraryFilename")

print(string.format("platform ok: %s/%s cpu=%d %s", osId, arch, platform.cpuCount(), endian))
