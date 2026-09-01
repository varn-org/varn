-- arrays iterate in order, maps iterate by key
local fruits = { "apple", "banana", "cherry" }

for index, name in ipairs(fruits) do
    print(index, name)
end

table.insert(fruits, "date")
table.sort(fruits)
print("sorted:", table.concat(fruits, ", "))

local counts = { apple = 3, banana = 5 }
for name, n in pairs(counts) do
    print(name, "=", n)
end
