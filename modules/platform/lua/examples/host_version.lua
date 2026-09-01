-- prints semver baked in at configure time for the host binary
local p = require("platform")

local v = p.hostVersion()
print("hostVersion", v)
assert(type(v) == "string" and #v > 0, "hostVersion")
assert(v:match("^%d+%.%d+%.%d+"), "expected semver x.y.z")
