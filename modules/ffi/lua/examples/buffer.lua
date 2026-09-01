-- allocates a c buffer, copies bytes in, casts it, and reads it back via ffi.string
local ffi = require("ffi")

local buf = ffi.new("char[?]", 6)
ffi.copy(buf, "world")

local ptr = ffi.cast("const char *", buf)
print("ffi buffer length:", ffi.sizeof("char[6]"))
print("ffi buffer text:", ffi.string(ptr))
print("ffi buffer prefix:", ffi.string(buf, 3))

