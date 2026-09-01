-- prints the number of cpus reported for the host
local p = require("platform")

local cpus = p.cpuCount()
print("cpuCount()", cpus)

assert(type(cpus) == "number" and cpus >= 1, "cpuCount should be at least one")
print("platform cpu count ok")
