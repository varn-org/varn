-- Declares enums whose constants are reachable through `ffi.C`, and uses an enum type sized like an int as an argument, a return value and a field.
local ffi = require("ffi")

ffi.cdef [[
    enum color { RED, GREEN = 4, BLUE };
    typedef enum { SHRINK = -1, KEEP, GROW = 0x10 } change_t;
    enum high { TOP = 0x80000000 };
    typedef struct { enum color tint; change_t change; } paint_t;
    typedef enum color (*next_color_fn)(enum color current);
]]

-- A constant without a value follows the one before it, and the first one is zero.
assert(ffi.C.RED == 0 and ffi.C.GREEN == 4 and ffi.C.BLUE == 5, 'The enum "color" should number its constants like C.')
assert(ffi.C.SHRINK == -1 and ffi.C.KEEP == 0 and ffi.C.GROW == 16, 'The enum "change_t" should take negative and hexadecimal values.')
assert(ffi.C.TOP == 0x80000000, 'The enum "high" should keep a value above the largest "int".')

-- An enum type is sized like an int, and one holding a value above the largest int is unsigned.
assert(ffi.sizeof("enum color") == ffi.sizeof("int"), 'The enum "color" should be as large as an "int".')
assert(ffi.sizeof("change_t") == ffi.sizeof("int"), 'The enum "change_t" should be as large as an "int".')
assert(ffi.sizeof("paint_t") == 2 * ffi.sizeof("int"), 'The struct "paint_t" should hold two enums.')
assert(ffi.tonumber(ffi.new("enum high", ffi.C.TOP)) == 0x80000000, 'The enum "high" should read back unsigned.')

-- A field of an enum type reads and writes its constants.
local paint = ffi.new("paint_t")
paint.tint = ffi.C.BLUE
paint.change = ffi.C.SHRINK
assert(paint.tint == ffi.C.BLUE and paint.change == -1, 'The struct "paint_t" should keep the constants written to it.')

-- An enum type passes as an argument and comes back as a return value.
local next_color = ffi.cast("next_color_fn", function(current)
    return current == ffi.C.RED and ffi.C.GREEN or ffi.C.BLUE
end)
assert(next_color(ffi.C.RED) == ffi.C.GREEN, 'The function "next_color_fn" should answer "GREEN" after "RED".')
assert(next_color(ffi.C.GREEN) == ffi.C.BLUE, 'The function "next_color_fn" should answer "BLUE" after "GREEN".')

-- A constant declared twice, an enum never declared and a value no "int" holds are refused by name.
local ok_twice, err_twice = pcall(ffi.cdef, "enum again { RED };")
assert(not ok_twice and err_twice:find('"RED"', 1, true), "A constant declared twice should be refused, not: " .. tostring(err_twice))

local ok_missing, err_missing = pcall(ffi.sizeof, "enum missing")
assert(not ok_missing and err_missing:find('"missing"', 1, true), "An enum never declared should be refused, not: " .. tostring(err_missing))

local ok_wide, err_wide = pcall(ffi.cdef, "enum wide { HUGE_VALUE = 0x100000000 };")
assert(not ok_wide and err_wide:find('"HUGE_VALUE"', 1, true), "A value no int holds should be refused, not: " .. tostring(err_wide))

local ok_mixed, err_mixed = pcall(ffi.cdef, "enum mixed { LOW = -1, HIGH = 0x80000000 };")
assert(not ok_mixed and err_mixed:find('"mixed"', 1, true), "An enum no int holds whole should be refused, not: " .. tostring(err_mixed))

print("The \"ffi\" enum tests passed.")
