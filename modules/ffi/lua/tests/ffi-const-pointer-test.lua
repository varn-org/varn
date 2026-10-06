-- Accepts a pointer or an array where a pointer to const is expected, and refuses a pointer to const where a pointer to a mutable value is expected.
local ffi = require("ffi")

ffi.cdef [[
    unsigned long strlen(const char *s);
    typedef int (*sum_fn)(const int *values, int count);
    typedef void (*clear_fn)(int *values, int count);
    typedef struct { const int *values; int *target; } view_t;
]]

-- An array of char and a pointer to char both reach "strlen", which takes a pointer to const char.
local text = ffi.new("char[16]", "hello")
assert(ffi.C.strlen(text) == 5, 'The function "strlen" should take a "char[16]".')

local pointer = ffi.cast("char *", text)
assert(ffi.C.strlen(pointer) == 5, 'The function "strlen" should take a "char *".')

-- A declared function taking a pointer to const int reads an array of int and a pointer to int.
local sum = ffi.cast("sum_fn", function(values, count)
    local total = 0
    for index = 0, count - 1 do
        total = total + values[index]
    end
    return total
end)

local values = ffi.new("int[4]", { 1, 2, 3, 4 })
assert(sum(values, 4) == 10, 'The function "sum_fn" should take an "int[4]".')
assert(sum(ffi.cast("int *", values), 4) == 10, 'The function "sum_fn" should take an "int *".')

-- A field that points at const takes a mutable array too.
local view = ffi.new("view_t")
view.values = values
assert(view.values[3] == 4, 'The field "values" should take an "int[4]".')

-- A pointer to const is refused where a pointer to a mutable value is expected, as an argument and as a field.
local clear = ffi.cast("clear_fn", function() end)
local readonly = ffi.cast("const int *", values)

local ok, err = pcall(clear, readonly, 4)
assert(not ok, 'The function "clear_fn" should refuse a "const int *".')
assert(err:find('"const int *"', 1, true) and err:find('"int *"', 1, true), "The refusal should name both types, not: " .. tostring(err))

local ok_field, err_field = pcall(function()
    view.target = readonly
end)
assert(not ok_field and err_field:find('"const int *"', 1, true), 'The field "target" should refuse a "const int *", not: ' .. tostring(err_field))

-- A pointer to void takes any pointer, a pointer to const included, as it does in C.
local untyped = ffi.new("void *", readonly)
assert(ffi.cast("const int *", untyped)[0] == 1, 'A "void *" should take a "const int *".')

-- A cast still reinterprets a pointer to const as a pointer to a mutable value.
assert(ffi.cast("int *", readonly)[0] == 1, 'The call "ffi.cast" should drop the const of a pointer.')

print("The \"ffi\" const pointer tests passed.")
