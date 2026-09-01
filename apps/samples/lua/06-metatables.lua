-- a metatable gives a table operators and methods
local Vector = {}
Vector.__index = Vector

function Vector.new(x, y)
    return setmetatable({ x = x, y = y }, Vector)
end

function Vector:length()
    return math.sqrt(self.x * self.x + self.y * self.y)
end

Vector.__add = function(a, b)
    return Vector.new(a.x + b.x, a.y + b.y)
end

Vector.__tostring = function(v)
    return string.format("(%g, %g)", v.x, v.y)
end

local a = Vector.new(3, 4)
local b = Vector.new(1, 2)
print("a:", tostring(a), "length:", a:length())
print("a + b:", tostring(a + b))
