-- pcall turns a runtime error into a value rather than unwinding
local ok, err = pcall(function()
    error("something went wrong")
end)
print("ok:", ok)
print("err:", err)

local divided, result = pcall(function(a, b)
    return a / b
end, 10, 2)
print("divided:", divided, "result:", result)
