-- declares a c struct, attaches methods with metatype, and inspects its layout with typeof, sizeof, offsetof, and istype
local ffi = require("ffi")

ffi.cdef [[
    typedef struct { int x; int y; } point_t;
]]

local point_t = ffi.typeof("point_t")
ffi.metatype(point_t, {
    __index = {
        sum = function(self)
            return self.x + self.y
        end,
    },
})

local p = ffi.new(point_t)
p.x = 3
p.y = 4

print("ffi struct sizeof:", ffi.sizeof("point_t"))
print("ffi struct offsetof y:", ffi.offsetof("point_t", "y"))
print("ffi struct istype:", ffi.istype(point_t, p))
print("ffi struct sum:", p:sum())
print("ffi struct ok")
