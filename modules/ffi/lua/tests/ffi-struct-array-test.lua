-- Lays out, reads, writes, sorts and passes by value structs and unions whose fields are arrays.
local ffi = require("ffi")

ffi.cdef [[
    typedef struct { int values[4]; char name[16]; } record_t;
    typedef struct { char tag; double samples[3]; } samples_t;
    typedef union { int values[4]; double wide; } overlay_t;
    typedef struct { float parts[4]; } quad_t;
    typedef record_t (*scale_fn)(record_t record, int factor);
    typedef quad_t (*swap_fn)(quad_t quad);
    void qsort(void *base, unsigned long count, unsigned long size, int (*compare)(const void *, const void *));
]]

-- An array field is laid out like its elements written one after the other.
assert(ffi.sizeof("record_t") == 4 * ffi.sizeof("int") + 16, 'The struct "record_t" should hold both arrays whole.')
assert(ffi.offsetof("record_t", "values") == 0, 'The field "values" should start the struct.')
assert(ffi.offsetof("record_t", "name") == 4 * ffi.sizeof("int"), 'The field "name" should follow the four ints.')

local spelled = "struct { char tag; double first; double second; double third; }"
assert(ffi.offsetof("samples_t", "samples") == ffi.offsetof(spelled, "first"), 'The field "samples" should be aligned like a "double".')
assert(ffi.sizeof("samples_t") == ffi.sizeof(spelled), 'The struct "samples_t" should be as large as its elements spelled out.')

-- A union takes the size of its largest member and the alignment of its strictest one.
assert(ffi.sizeof("overlay_t") == 4 * ffi.sizeof("int"), 'The union "overlay_t" should be as large as its array.')
assert(ffi.offsetof("struct { char tag; overlay_t overlay; }", "overlay") == ffi.offsetof("struct { char tag; double wide; }", "wide"), 'The union "overlay_t" should be aligned like a "double".')

-- The elements of an array field read and write in place.
local record = ffi.new("record_t")
record.values[2] = 7
record.name = "knight"
assert(record.values[2] == 7 and record.values[0] == 0, 'The field "values" should keep what was written.')
assert(ffi.string(record.name) == "knight", 'The field "name" should keep the copied text.')
assert(#record.values == 4, 'The field "values" should report four elements.')

local initialized = ffi.new("record_t", { { 1, 2, 3, 4 }, "archer" })
assert(initialized.values[3] == 4 and ffi.string(initialized.name) == "archer", 'The struct "record_t" should take nested initializers.')

-- A callback sorts an array of such structs through pointers to them.
local records = ffi.new("record_t[3]")
for index, value in ipairs({ 30, 10, 20 }) do
    records[index - 1].values[0] = value
    records[index - 1].name = "record " .. value
end

local compare = ffi.cast("int (*)(const void *, const void *)", function(a, b)
    local left = ffi.cast("const record_t *", a).values[0]
    local right = ffi.cast("const record_t *", b).values[0]
    return left < right and -1 or (left > right and 1 or 0)
end)

ffi.C.qsort(records, 3, ffi.sizeof("record_t"), compare)
assert(records[0].values[0] == 10 and records[2].values[0] == 30, 'The function "qsort" should sort by the first value.')
assert(ffi.string(records[0].name) == "record 10", 'The function "qsort" should move each struct whole.')

-- A struct holding arrays passes by value into a function and comes back by value from it.
local scale = ffi.cast("scale_fn", function(input, factor)
    local output = ffi.new("record_t")
    for index = 0, 3 do
        output.values[index] = input.values[index] * factor
    end
    output.name = ffi.string(input.name) .. "!"
    return output
end)

local scaled = scale(initialized, 3)
assert(scaled.values[0] == 3 and scaled.values[3] == 12, 'The function "scale_fn" should scale every value it received.')
assert(ffi.string(scaled.name) == "archer!", 'The function "scale_fn" should return the name it built.')

local swap = ffi.cast("swap_fn", function(input)
    return ffi.new("quad_t", { { input.parts[3], input.parts[2], input.parts[1], input.parts[0] } })
end)

local swapped = swap(ffi.new("quad_t", { { 1.5, 2.5, 3.5, 4.5 } }))
assert(swapped.parts[0] == 4.5 and swapped.parts[3] == 1.5, 'The function "swap_fn" should return the parts reversed.')

print("The \"ffi\" struct array tests passed.")
