-- Exercises `cdef`, calls, `new`, `cast`, `copy`, `string`, and introspection over the process namespace `ffi.C` with standard libc symbols.
local ffi = require("ffi")

ffi.cdef [[
    unsigned long strlen(const char *s);
    int abs(int n);
    int memcmp(const char *a, const char *b, unsigned long n);
    typedef struct { int x; int y; } point_t;
]]

-- Calls return deterministic results from libc.
assert(ffi.C.strlen("hello") == 5, 'The call "strlen(hello)" should be 5')
assert(ffi.C.strlen("") == 0, [[The call "strlen('')" should be 0]])
assert(ffi.C.abs(-3) == 3, 'The call "abs(-3)" should be 3')
assert(ffi.C.abs(7) == 7, 'The call "abs(7)" should be 7')

-- The `memcmp` call reports equality and difference.
assert(ffi.C.memcmp("abc", "abc", 3) == 0, 'The call "memcmp" of equal buffers should be 0')
assert(ffi.C.memcmp("abc", "abd", 3) ~= 0, 'The call "memcmp" of different buffers should be non-zero')

-- Introspection works over standard C types.
assert(ffi.sizeof("char") == 1, 'The call "sizeof" of "char" should be 1')
assert(ffi.sizeof("char[10]") == 10, 'The call "sizeof" of "char[10]" should be 10')
assert(tostring(ffi.typeof("int")) ~= nil, 'The call "typeof" of "int" should produce a ctype')

-- The `new` call allocates a buffer, `copy` fills it, and `string` reads it back.
local buf = ffi.new("char[?]", 6)
ffi.copy(buf, "world")
assert(ffi.string(buf) == "world", 'The call "ffi.string" should read back the copied bytes')

-- The `cast` call reinterprets the buffer as a const char pointer.
local ptr = ffi.cast("const char *", buf)
assert(ffi.string(ptr) == "world", 'The call "ffi.string" through a cast pointer should match')
assert(ffi.string(buf, 3) == "wor", 'The call "ffi.string" with an explicit length should truncate')

-- The null sentinel is exposed and stringifies.
assert(tostring(ffi.nullptr) ~= nil, 'The value "ffi.nullptr" should stringify')

-- The backend version is a non-empty string.
assert(type(ffi.VERSION) == "string" and #ffi.VERSION > 0, 'The field "ffi.VERSION" should be a non-empty string')

-- The `offsetof` call reports field offsets over standard types.
assert(ffi.offsetof("point_t", "x") == 0, 'The call "offsetof" of "x" should be 0')
assert(ffi.offsetof("point_t", "y") == ffi.sizeof("int"), 'The call "offsetof" of "y" should follow one int')

-- The `typeof` call produces a ctype that `new` and `istype` agree on.
local point_t = ffi.typeof("point_t")
local pt = ffi.new(point_t)
pt.x = 3
pt.y = 4
assert(ffi.istype(point_t, pt) == true, 'The call "istype" should match the constructing ctype')
assert(ffi.istype("int", pt) == false, 'The call "istype" should reject an unrelated ctype')

-- The `metatype` call attaches methods to the ctype's instances.
ffi.metatype(point_t, {
    __index = {
        sum = function(self)
            return self.x + self.y
        end,
    },
})
local pm = ffi.new(point_t)
pm.x = 10
pm.y = 11
assert(pm:sum() == 21, 'The "metatype" method should see the struct fields')

-- The `tonumber` call converts a cdata scalar to a Lua number.
local boxed = ffi.new("int", 42)
assert(ffi.tonumber(boxed) == 42, 'The call "tonumber" should unbox a cdata int')

-- The `fill` call writes a constant byte across a buffer.
local fbuf = ffi.new("unsigned char[?]", 4)
ffi.fill(fbuf, 4, 0x41)
assert(ffi.string(fbuf, 4) == "AAAA", 'The call "fill" should set every byte')

-- The `addressof` call yields a non-null pointer to a live cdata object.
assert(ffi.addressof(fbuf) ~= ffi.nullptr, 'The call "addressof" of a buffer should not be null')

-- The `errno` call reads back as a number.
ffi.C.abs(-1)
assert(type(ffi.errno()) == "number", 'The call "errno" should read back as a number')

-- The `gc` call registers a finalizer and returns the same cdata.
local owned = ffi.new("char[?]", 1)
assert(ffi.gc(owned, function() end) ~= nil, 'The call "gc" should return the guarded cdata')

print("The \"ffi\" feature tests passed.")

-- A callback answering an int reaches its caller whole, a negative one included, as `qsort` shows by sorting with it.
ffi.cdef [[
    void qsort(void *base, unsigned long count, unsigned long size, int (*compare)(const void *, const void *));
]]

local values = ffi.new("int[5]")
for index, value in ipairs({ 5, -3, 9, 0, -7 }) do
    values[index - 1] = value
end

local compare = ffi.cast("int (*)(const void *, const void *)", function(a, b)
    local left = ffi.cast("const int *", a)[0]
    local right = ffi.cast("const int *", b)[0]
    return left < right and -1 or (left > right and 1 or 0)
end)

ffi.C.qsort(values, 5, ffi.sizeof("int"), compare)
assert(values[0] == -7 and values[1] == -3 and values[2] == 0 and values[3] == 5 and values[4] == 9, 'The function "qsort" should sort with the answers of an int callback.')
