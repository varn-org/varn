-- Treats `intptr_t` and `uintptr_t` as integers as wide as a pointer, which hold an address and pass to functions.
local ffi = require("ffi")

ffi.cdef [[
    typedef struct { uintptr_t address; intptr_t offset; } span_t;
    typedef intptr_t (*negate_fn)(intptr_t value);
]]

-- Both types are as wide as a pointer.
assert(ffi.sizeof("intptr_t") == ffi.sizeof("void *"), 'The type "intptr_t" should be as wide as a pointer.')
assert(ffi.sizeof("uintptr_t") == ffi.sizeof("void *"), 'The type "uintptr_t" should be as wide as a pointer.')
assert(ffi.sizeof("span_t") == 2 * ffi.sizeof("void *"), 'The struct "span_t" should hold two pointer-wide integers.')

-- A signed one keeps its sign.
assert(ffi.tonumber(ffi.new("intptr_t", -5)) == -5, 'The type "intptr_t" should keep a negative value.')
assert(ffi.tonumber(ffi.new("uintptr_t", 7)) == 7, 'The type "uintptr_t" should keep a positive value.')

-- An address converts to an integer and back.
local buffer = ffi.new("int[1]", { 42 })
local address = ffi.cast("uintptr_t", buffer)
assert(ffi.cast("int *", address)[0] == 42, 'The type "uintptr_t" should hold an address that converts back to a pointer.')

local span = ffi.new("span_t")
span.address = address
span.offset = -16
assert(ffi.cast("int *", span.address)[0] == 42 and span.offset == -16, 'The struct "span_t" should keep its fields.')

-- Both pass to a function and come back from it.
local negate = ffi.cast("negate_fn", function(value)
    return -value
end)
assert(negate(-9) == 9 and negate(12) == -12, 'The function "negate_fn" should take and return an "intptr_t".')

print("The \"ffi\" intptr tests passed.")
