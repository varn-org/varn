-- Every documented feature returns the right type and a plausible value without assuming the host OS.
local platform = require("platform")

-- The `os` call returns a non-empty string from the documented set.
local osId = platform.os()
assert(type(osId) == "string" and #osId > 0, 'The call "os" should return a non-empty string')
local knownOs = { linux = true, macos = true, windows = true, ios = true, android = true, wasm = true }
assert(knownOs[osId], 'The call "os" should return one of the documented identifiers')

-- The `arch` call returns a non-empty string from the documented set.
local arch = platform.arch()
assert(type(arch) == "string" and #arch > 0, 'The call "arch" should return a non-empty string')
local knownArch = { arm64 = true, x86_64 = true, wasm32 = true, arm = true, x86 = true }
assert(knownArch[arch], 'The call "arch" should return one of the documented identifiers')

-- The `hostVersion` field is a non-empty semver string baked in at configure time.
local hostVersion = platform.hostVersion()
assert(type(hostVersion) == "string" and #hostVersion > 0, 'The call "hostVersion" should return a non-empty string')
assert(hostVersion:match("^%d+%.%d+%.%d+"), 'The call "hostVersion" should return a semver "x.y.z"')

-- The same version is exposed as numbers so a component can gate on it without parsing the string itself.
local version = platform.version
assert(type(version) == "table", 'The field "version" should be a table')
for _, field in ipairs({ "major", "minor", "patch" }) do
    assert(math.type(version[field]) == "integer", 'The field "version.' .. field .. '" should be an integer')
    assert(version[field] >= 0, 'The field "version.' .. field .. '" should not be negative')
end
assert(version.string == hostVersion, 'The field "version.string" should match "hostVersion()"')
assert(string.format("%d.%d.%d", version.major, version.minor, version.patch) == hostVersion:match("^(%d+%.%d+%.%d+)"),
    "The numeric fields should reconstruct the version string")

-- The CPU count is a positive integer.
local cpus = platform.cpuCount()
assert(type(cpus) == "number" and cpus >= 1, 'The call "cpuCount" should return at least one')

-- The pointer size is four or eight bytes.
local pointer = platform.pointerSize()
assert(pointer == 4 or pointer == 8, 'The call "pointerSize" should return four or eight')

-- The endianness is little or big.
local endian = platform.endianness()
assert(endian == "little" or endian == "big", 'The call "endianness" should return "little" or "big"')

-- Library naming pieces are strings and the suffix is non-empty.
assert(type(platform.libPrefix()) == "string", 'The call "libPrefix" should return a string')
local suffix = platform.shlibSuffix()
assert(type(suffix) == "string" and #suffix > 0, 'The call "shlibSuffix" should return a non-empty string')

-- The `libraryFilename` call returns a non-empty string that embeds the logical name.
local filename = platform.libraryFilename("sqlite3")
assert(type(filename) == "string" and #filename > 0, 'The call "libraryFilename" should return a non-empty string')
assert(filename:find("sqlite3", 1, true), 'The call "libraryFilename" should contain the logical name')

-- The `getLibraryPathByName` call builds a path that ends with the filename when no subdir is given.
local defaultPath = platform.getLibraryPathByName("sqlite3")
assert(type(defaultPath) == "string" and #defaultPath > 0, 'The call "getLibraryPathByName" should return a default path')
assert(defaultPath:find(filename, 1, true), "The default path should contain the filename")

-- The `getLibraryPathByName` call prefixes the subdir when one is supplied.
local subPath = platform.getLibraryPathByName("sqlite3", "vendor/libs")
assert(type(subPath) == "string" and #subPath > 0, 'The call "getLibraryPathByName" should return a subdir path')
assert(subPath:find("vendor/libs", 1, true), "The subdir path should contain the subdir")
assert(subPath:find(filename, 1, true), "The subdir path should contain the filename")

-- Repeated calls stay consistent within a single run.
assert(platform.os() == osId, 'The call "os" should be stable across calls')
assert(platform.arch() == arch, 'The call "arch" should be stable across calls')
assert(platform.pointerSize() == pointer, 'The call "pointerSize" should be stable across calls')

print("The \"platform\" features tests passed.")
