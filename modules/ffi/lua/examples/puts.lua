-- calls libc puts through the native ffi stack
local ffi = require("ffi")

ffi.cdef [[
    int puts(const char *s);
]]

ffi.C.puts("ffi: hello from libc puts")

