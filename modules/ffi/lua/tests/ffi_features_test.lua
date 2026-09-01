-- exercises cdef, calls, new, cast, copy, string, and introspection over the process namespace ffi.C with standard libc symbols
local ffi = require("ffi")

ffi.cdef [[
    unsigned long strlen(const char *s);
    int abs(int n);
    int memcmp(const char *a, const char *b, unsigned long n);
    typedef struct { int x; int y; } point_t;
]]

-- calls return deterministic results from libc
assert(ffi.C.strlen("hello") == 5, "strlen(hello) should be 5")
assert(ffi.C.strlen("") == 0, "strlen('') should be 0")
assert(ffi.C.abs(-3) == 3, "abs(-3) should be 3")
assert(ffi.C.abs(7) == 7, "abs(7) should be 7")

-- memcmp reports equality and difference
assert(ffi.C.memcmp("abc", "abc", 3) == 0, "memcmp of equal buffers should be 0")
assert(ffi.C.memcmp("abc", "abd", 3) ~= 0, "memcmp of different buffers should be non-zero")

-- introspection over standard c types
assert(ffi.sizeof("char") == 1, "sizeof char should be 1")
assert(ffi.sizeof("char[10]") == 10, "sizeof char[10] should be 10")
assert(tostring(ffi.typeof("int")) ~= nil, "typeof int should produce a ctype")

-- new allocates a buffer, copy fills it, and string reads it back
local buf = ffi.new("char[?]", 6)
ffi.copy(buf, "world")
assert(ffi.string(buf) == "world", "ffi.string should read back the copied bytes")

-- cast reinterprets the buffer as a const char pointer
local ptr = ffi.cast("const char *", buf)
assert(ffi.string(ptr) == "world", "ffi.string through a cast pointer should match")
assert(ffi.string(buf, 3) == "wor", "ffi.string with an explicit length should truncate")

-- the null sentinel is exposed and stringifies
assert(tostring(ffi.nullptr) ~= nil, "ffi.nullptr should stringify")

-- the backend version is a non-empty string
assert(type(ffi.VERSION) == "string" and #ffi.VERSION > 0, "ffi.VERSION should be a non-empty string")

-- offsetof reports field offsets over standard types
assert(ffi.offsetof("point_t", "x") == 0, "offsetof x should be 0")
assert(ffi.offsetof("point_t", "y") == ffi.sizeof("int"), "offsetof y should follow one int")

-- typeof produces a ctype that new and istype agree on
local point_t = ffi.typeof("point_t")
local pt = ffi.new(point_t)
pt.x = 3
pt.y = 4
assert(ffi.istype(point_t, pt) == true, "istype should match the constructing ctype")
assert(ffi.istype("int", pt) == false, "istype should reject an unrelated ctype")

-- metatype attaches methods to the ctype's instances
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
assert(pm:sum() == 21, "metatype method should see the struct fields")

-- tonumber converts a cdata scalar to a lua number
local boxed = ffi.new("int", 42)
assert(ffi.tonumber(boxed) == 42, "tonumber should unbox a cdata int")

-- fill writes a constant byte across a buffer
local fbuf = ffi.new("unsigned char[?]", 4)
ffi.fill(fbuf, 4, 0x41)
assert(ffi.string(fbuf, 4) == "AAAA", "fill should set every byte")

-- addressof yields a non-null pointer to a live cdata object
assert(ffi.addressof(fbuf) ~= ffi.nullptr, "addressof of a buffer should not be null")

-- errno reads back as a number
ffi.C.abs(-1)
assert(type(ffi.errno()) == "number", "errno should read back as a number")

-- gc registers a finalizer and returns the same cdata
local owned = ffi.new("char[?]", 1)
assert(ffi.gc(owned, function() end) ~= nil, "gc should return the guarded cdata")

print("ffi features ok")
