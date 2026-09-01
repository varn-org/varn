-- patterns, iteration over matches and substitution
local text = "varn: fast, small, embeddable"

for word in text:gmatch("%a+") do
    print(word)
end

print("upper:", text:upper())
print("commas:", select(2, text:gsub(",", "")))
print("format:", string.format("%s has %d chars", text, #text))
