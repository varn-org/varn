-- encodes non-finite numbers as null instead of throwing
local json = require("json")

-- encodes nan and both infinities as null
print(json.encode({ nan = 0 / 0, pos = 1 / 0, neg = -1 / 0 }))

-- keeps finite values in a mixed array and nulls out the rest
print(json.encode({ 1, 1 / 0, 2.5, 0 / 0 }))
