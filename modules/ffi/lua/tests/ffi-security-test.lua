-- Covers argument type validation the binding performs over the process namespace `ffi.C` with standard libc symbols, exercising FFI-023 type confusion of a string versus a pointer and FFI-028 float or int coercion to a wrong C type.
local ffi = require("ffi")

ffi.cdef [[
    unsigned long strlen(const char *s);
    int abs(int n);
]]

-- FFI-023: Passing a table where a const char pointer is expected is rejected.
local ok_table, err_table = pcall(function()
    return ffi.C.strlen({})
end)
assert(ok_table == false, 'The call "strlen" with a table argument should be rejected')
assert(type(err_table) == "string", "The rejection should carry an error message")

-- FFI-028: Passing a string where an int is expected is rejected.
local ok_str, err_str = pcall(function()
    return ffi.C.abs("not a number")
end)
assert(ok_str == false, 'The call "abs" with a string argument should be rejected')
assert(type(err_str) == "string", "The rejection should carry an error message")

-- Valid calls succeed alongside the rejected ones.
assert(ffi.C.strlen("ok") == 2, "Valid calls should still succeed after a rejected one")
assert(ffi.C.abs(-9) == 9, "Valid calls should still succeed after a rejected one")

print("The \"ffi\" security tests passed.")
