-- percent-encodes a value for a query string and decodes it back, available in every build including the browser
local http = require("http")

local encoded = http.urlEncode("hello world & more")
print("encoded " .. encoded)

local decoded = http.urlDecode(encoded)
print("decoded " .. decoded)

assert(decoded == "hello world & more", "round-trip mismatch")

print("http url encode ok")
