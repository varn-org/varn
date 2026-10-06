-- Calls through pointers to functions, cast from the address of a libc function or read from a struct field, and refuses a null one.
local ffi = require("ffi")

ffi.cdef [[
    int abs(int n);
    typedef struct { int (*apply)(int); int value; } operation_t;
]]

-- The address of a libc function cast to a pointer answers like the function itself.
local abs_pointer = ffi.cast("int (*)(int)", ffi.C.abs)
assert(abs_pointer(-5) == 5, 'The pointer to "abs" should answer 5 for -5.')
assert(abs_pointer(7) == 7, 'The pointer to "abs" should answer 7 for 7.')

-- A struct field holding a function pointer is called through the field.
local operation = ffi.new("operation_t")
operation.apply = abs_pointer
operation.value = -8
assert(operation.apply(operation.value) == 8, 'The field "apply" should call "abs" through its pointer.')

operation.apply = ffi.C.abs
assert(operation.apply(-9) == 9, 'The field "apply" should take the function "abs" itself.')

-- A pointer to a Lua callback read back from a field calls the callback.
local double = ffi.cast("int (*)(int)", function(n)
    return n * 2
end)
operation.apply = double
assert(operation.apply(21) == 42, 'The field "apply" should call the Lua callback it holds.')

-- A pointer through a pointer to the struct calls the same function.
local through = ffi.cast("operation_t *", ffi.addressof(operation))
assert(through.apply(4) == 8, 'The field "apply" read through a pointer should call the callback.')

-- A null function pointer refuses the call and says the pointer is null.
local null_pointer = ffi.new("int (*)(int)")
local ok, err = pcall(null_pointer, 1)
assert(not ok, "A null function pointer should refuse the call.")
assert(err:find("null", 1, true), "The refusal should say the pointer is null, not: " .. tostring(err))
assert(err:find('"int (*)(int)"', 1, true), "The refusal should name the pointer type, not: " .. tostring(err))

operation.apply = nil
local ok_field, err_field = pcall(function()
    return operation.apply(1)
end)
assert(not ok_field and err_field:find("null", 1, true), "A null field should refuse the call, not: " .. tostring(err_field))

-- A value that is not a function stays uncallable.
local ok_value, err_value = pcall(ffi.new("int", 1))
assert(not ok_value and err_value:find('"int"', 1, true), 'An "int" should refuse a call, not: ' .. tostring(err_value))

-- A cast to a function type it already knows shares that type, so casting again and again keeps no new memory.
local function heapAfter(casts)
    for _ = 1, casts do
        ffi.cast("int (*)(int)", ffi.C.abs)
    end
    collectgarbage("collect")
    collectgarbage("collect")
    return collectgarbage("count")
end

local settled = heapAfter(2000)
local grown = heapAfter(2000) - settled
assert(grown < 16, "Repeating a cast to a known function type should keep no memory, yet the heap grew by " .. grown .. " KiB.")

print("The \"ffi\" function pointer tests passed.")
