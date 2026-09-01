-- converts types between lua and json in both directions
local json = require("json")

-- encodes scalars and containers directly
print(json.encode("a string"))
print(json.encode(42))
print(json.encode(3.5))
print(json.encode(true))
print(json.encode({}))
print(json.encode({ 1, 2, 3 }))
print(json.encode({ nested = { a = 1, b = { 2, 3 } } }))

-- decodes json types onto lua values
local v = json.decode('{"i":7,"f":1.5,"b":false,"arr":[1,2],"obj":{"k":"v"}}')
print(v.i, v.f, v.b, v.arr[2], v.obj.k)
