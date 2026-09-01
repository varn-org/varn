-- http url percent-encoding covering component encode and a decode that also turns form '+' into a space
local http = require("http")

assert(http.urlEncode("a b&c=d/e") == "a%20b%26c%3Dd%2Fe", "component encode")
assert(http.urlEncode("safe-_.~AZ09") == "safe-_.~AZ09", "unreserved characters pass through")
assert(http.urlEncode("\195\161") == "%C3%A1", "utf-8 bytes are escaped")

assert(http.urlDecode("a%20b%2Bc+d") == "a b+c d", "decodes percent escapes and form plus")
assert(http.urlDecode("%C3%A1") == "\195\161", "decodes multi-byte utf-8")

-- a round-trip survives arbitrary binary, including a null byte
local raw = "hello \0 w/x? = &"
assert(http.urlDecode(http.urlEncode(raw)) == raw, "binary round-trip")

print("http url ok")
