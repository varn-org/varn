-- declares a libc symbol and calls it for a deterministic result
local ffi = require("ffi")

ffi.cdef [[
    unsigned long strlen(const char *s);
]]

assert(ffi.C.strlen("hello") == 5, "strlen(hello) should be 5")
assert(ffi.C.strlen("") == 0, "strlen('') should be 0")

print("ffi ok")
