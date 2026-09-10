-- Every documented feature returns the right type and a plausible value without assuming the host os.
local platform = require("platform")

-- Os is a non-empty string from the documented set.
local osId = platform.os()
assert(type(osId) == "string" and #osId > 0, "os")
local knownOs = { linux = true, macos = true, windows = true, ios = true, android = true, wasm = true }
assert(knownOs[osId], "os should be one of the documented identifiers")

-- Arch is a non-empty string from the documented set.
local arch = platform.arch()
assert(type(arch) == "string" and #arch > 0, "arch")
local knownArch = { arm64 = true, x86_64 = true, wasm32 = true, arm = true, x86 = true }
assert(knownArch[arch], "arch should be one of the documented identifiers")

-- The hostVersion field is a non-empty semver string baked in at configure time.
local hostVersion = platform.hostVersion()
assert(type(hostVersion) == "string" and #hostVersion > 0, "hostVersion")
assert(hostVersion:match("^%d+%.%d+%.%d+"), "hostVersion should be semver x.y.z")

-- The same version is exposed as numbers so a component can gate on it without parsing the string itself.
local version = platform.version
assert(type(version) == "table", "version should be a table")
for _, field in ipairs({ "major", "minor", "patch" }) do
    assert(math.type(version[field]) == "integer", "version." .. field .. " should be an integer")
    assert(version[field] >= 0, "version." .. field .. " should not be negative")
end
assert(version.string == hostVersion, "version.string should match hostVersion()")
assert(string.format("%d.%d.%d", version.major, version.minor, version.patch) == hostVersion:match("^(%d+%.%d+%.%d+)"),
    "the numeric fields should reconstruct the version string")

-- Cpu count is a positive integer.
local cpus = platform.cpuCount()
assert(type(cpus) == "number" and cpus >= 1, "cpuCount should be at least one")

-- Pointer size is four or eight bytes.
local pointer = platform.pointerSize()
assert(pointer == 4 or pointer == 8, "pointerSize")

-- Endianness is little or big.
local endian = platform.endianness()
assert(endian == "little" or endian == "big", "endianness")

-- Library naming pieces are strings and the suffix is non-empty.
assert(type(platform.libPrefix()) == "string", "libPrefix")
local suffix = platform.shlibSuffix()
assert(type(suffix) == "string" and #suffix > 0, "shlibSuffix")

-- The libraryFilename call returns a non-empty string that embeds the logical name.
local filename = platform.libraryFilename("sqlite3")
assert(type(filename) == "string" and #filename > 0, "libraryFilename")
assert(filename:find("sqlite3", 1, true), "libraryFilename should contain the logical name")

-- The getLibraryPathByName call builds a path that ends with the filename when no subdir is given.
local defaultPath = platform.getLibraryPathByName("sqlite3")
assert(type(defaultPath) == "string" and #defaultPath > 0, "getLibraryPathByName default")
assert(defaultPath:find(filename, 1, true), "default path should contain the filename")

-- The getLibraryPathByName call prefixes the subdir when one is supplied.
local subPath = platform.getLibraryPathByName("sqlite3", "vendor/libs")
assert(type(subPath) == "string" and #subPath > 0, "getLibraryPathByName subdir")
assert(subPath:find("vendor/libs", 1, true), "subdir path should contain the subdir")
assert(subPath:find(filename, 1, true), "subdir path should contain the filename")

-- Repeated calls stay consistent within a single run.
assert(platform.os() == osId, "os should be stable across calls")
assert(platform.arch() == arch, "arch should be stable across calls")
assert(platform.pointerSize() == pointer, "pointerSize should be stable across calls")

print("platform features ok")
