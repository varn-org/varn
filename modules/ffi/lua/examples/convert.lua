-- fills a buffer, converts a cdata number to lua, takes a pointer address, reads errno, registers a gc finalizer, and prints the backend version
local ffi = require("ffi")

ffi.cdef [[
    int abs(int n);
]]

local buf = ffi.new("unsigned char[?]", 4)
ffi.fill(buf, 4, 0x41)
print("ffi convert fill:", ffi.string(buf, 4))

local n = ffi.new("int", 65)
print("ffi convert tonumber:", ffi.tonumber(n))
print("ffi convert addressof:", ffi.addressof(buf) ~= ffi.nullptr)

ffi.C.abs(-1)
print("ffi convert errno is number:", type(ffi.errno()) == "number")

local owned = ffi.new("char[?]", 1)
ffi.gc(owned, function() end)

print("ffi convert version:", ffi.VERSION)
print("ffi convert ok")
