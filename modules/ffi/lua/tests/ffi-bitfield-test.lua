-- Lays out, reads and writes bitfields the way the C compiler of the platform does, passes them to functions and refuses one wider than its type.
local ffi = require("ffi")
local platform = require("platform")

ffi.cdef [[
    typedef struct { unsigned flags : 3; unsigned mode : 5; int level : 4; unsigned rest : 20; } header_t;
    typedef struct { unsigned first : 30; unsigned second : 4; } split_t;
    typedef struct { unsigned char tag; unsigned value : 4; } mixed_t;
    typedef union { unsigned bits : 3; unsigned char whole; } overlay_t;
    typedef void (*touch_fn)(header_t *header);
    typedef int (*level_fn)(header_t header);
]]

-- A bare "unsigned" and a "signed char" are types of their own, never taken for "int" or "unsigned char".
assert(tostring(ffi.typeof("unsigned")) == "ctype<unsigned int>", 'The type "unsigned" should be an "unsigned int".')
assert(ffi.tonumber(ffi.new("signed char", -1)) == -1 and ffi.tonumber(ffi.new("unsigned char", 255)) == 255, 'The types "signed char" and "unsigned char" should keep their signs.')

-- Bitfields of one type share its storage until one no longer fits.
assert(ffi.sizeof("header_t") == ffi.sizeof("unsigned"), 'The struct "header_t" should fit one "unsigned".')
assert(ffi.sizeof("split_t") == 2 * ffi.sizeof("unsigned"), 'The field "second" should start a new "unsigned".')

local offset, bit, width = ffi.offsetof("header_t", "mode")
assert(offset == 0 and bit == 3 and width == 5, 'The call "ffi.offsetof" should give the byte, the bit and the width of "mode".')

-- A bitfield after a field of another type shares its storage on System V and starts a new one on Windows.
local windows = platform.os() == "windows"
assert(ffi.sizeof("mixed_t") == (windows and 8 or 4), 'The struct "mixed_t" should be laid out like the C compiler of the platform.')
assert(ffi.offsetof("mixed_t", "value") == (windows and 4 or 0), 'The field "value" should sit where the C compiler of the platform puts it.')

-- Each bitfield reads back what was written, masked to its width, and a signed one extends its sign.
local header = ffi.new("header_t")
header.flags = 5
header.mode = 17
header.level = -3
header.rest = 0xFFFFF
assert(header.flags == 5 and header.mode == 17 and header.level == -3 and header.rest == 0xFFFFF, 'The struct "header_t" should keep every bitfield.')

header.flags = 9
assert(header.flags == 1 and header.mode == 17, 'The field "flags" should keep only its three bits and leave "mode" alone.')

local bits = ffi.cast("unsigned *", ffi.addressof(header))[0]
assert(bits & 0xFF == 1 | (17 << 3), 'The bitfields should sit in the bits the C compiler gives them.')

local initialized = ffi.new("header_t", { 2, 3, -1, 4 })
assert(initialized.flags == 2 and initialized.mode == 3 and initialized.level == -1 and initialized.rest == 4, 'The struct "header_t" should take initializers for its bitfields.')

-- A bitfield in a union overlays the start of the other members.
local overlay = ffi.new("overlay_t")
overlay.whole = 0xFF
assert(overlay.bits == 7, 'The bitfield "bits" should read the low bits of the union.')
assert(ffi.sizeof("overlay_t") == ffi.sizeof("unsigned"), 'The union "overlay_t" should be as large as its "unsigned".')

-- A struct of bitfields passes to a function by pointer and by value.
local touch = ffi.cast("touch_fn", function(target)
    target.mode = target.mode + 1
end)
touch(header)
assert(header.mode == 18, 'The function "touch_fn" should change "mode" through the pointer.')

local level = ffi.cast("level_fn", function(value)
    return value.level
end)
assert(level(header) == -3, 'The function "level_fn" should read "level" from the struct it received.')

-- A bitfield wider than its type, one of no width and one of a type that is not an integer are refused.
local ok_wide, err_wide = pcall(ffi.cdef, "struct too_wide { unsigned char flags : 9; };")
assert(not ok_wide and err_wide:find('"flags"', 1, true) and err_wide:find('"unsigned char"', 1, true), "A bitfield wider than its type should be refused, not: " .. tostring(err_wide))

local ok_zero, err_zero = pcall(ffi.cdef, "struct no_width { unsigned flags : 0; };")
assert(not ok_zero and err_zero:find('"flags"', 1, true), "A bitfield of no width should be refused, not: " .. tostring(err_zero))

local ok_float, err_float = pcall(ffi.cdef, "struct not_integer { double ratio : 3; };")
assert(not ok_float and err_float:find('"ratio"', 1, true) and err_float:find('"double"', 1, true), "A bitfield of a type that is not an integer should be refused, not: " .. tostring(err_float))

print("The \"ffi\" bitfield tests passed.")
