-- covers argument type validation the binding performs over the process namespace ffi.C with standard libc symbols, exercising FFI-023 type confusion string vs pointer and FFI-028 float/int coercion to a wrong c type
local ffi = require("ffi")

ffi.cdef [[
    unsigned long strlen(const char *s);
    int abs(int n);
]]

-- FFI-023 passing a table where a const char pointer is expected is rejected
local ok_table, err_table = pcall(function()
    return ffi.C.strlen({})
end)
assert(ok_table == false, "strlen with a table arg should be rejected")
assert(type(err_table) == "string", "the rejection should carry an error message")

-- FFI-028 passing a string where an int is expected is rejected
local ok_str, err_str = pcall(function()
    return ffi.C.abs("not a number")
end)
assert(ok_str == false, "abs with a string arg should be rejected")
assert(type(err_str) == "string", "the rejection should carry an error message")

-- valid calls succeed alongside the rejected ones
assert(ffi.C.strlen("ok") == 2, "valid calls should still succeed after a rejected one")
assert(ffi.C.abs(-9) == 9, "valid calls should still succeed after a rejected one")

print("ffi security ok")
