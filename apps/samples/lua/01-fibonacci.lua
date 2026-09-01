-- plain lua, no modules: recursion, string.format and a loop
local function fib(n)
    if n < 2 then
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

print("fib(12) =", fib(12))

for i = 1, 4 do
    print(string.format("loop %d", i))
end
