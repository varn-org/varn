-- Platform host identifiers and system data are present and well-formed.
local platform = require("platform")

local osId = platform.os()
assert(type(osId) == "string" and #osId > 0, 'The call "os" should return a non-empty string')

local arch = platform.arch()
assert(type(arch) == "string" and #arch > 0, 'The call "arch" should return a non-empty string')

assert(type(platform.hostVersion()) == "string", 'The call "hostVersion" should return a string')
assert(platform.cpuCount() >= 1, 'The call "cpuCount" should return at least one')

local pointer = platform.pointerSize()
assert(pointer == 4 or pointer == 8, 'The call "pointerSize" should return four or eight')

local endian = platform.endianness()
assert(endian == "little" or endian == "big", 'The call "endianness" should return "little" or "big"')

assert(#platform.shlibSuffix() > 0, 'The call "shlibSuffix" should return a non-empty string')
assert(type(platform.libPrefix()) == "string", 'The call "libPrefix" should return a string')
assert(#platform.libraryFilename("sqlite3") > 0, 'The call "libraryFilename" should return a non-empty string')

print(string.format("The \"platform\" tests passed: %s/%s cpu=%d %s.", osId, arch, platform.cpuCount(), endian))
