-- prints the host operating system and cpu architecture identifiers
local p = require("platform")

print("os()  ", p.os())
print("arch()", p.arch())

assert(type(p.os()) == "string" and #p.os() > 0, "os")
assert(type(p.arch()) == "string" and #p.arch() > 0, "arch")
print("platform system identity ok")
