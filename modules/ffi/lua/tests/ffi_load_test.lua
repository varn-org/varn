-- Refuses a library that cannot be loaded with a message that names it and gives the reason of the system.
local ffi = require("ffi")

local ok, err = pcall(ffi.load, "varn-no-such-library")
assert(not ok, 'The call "ffi.load" should refuse a library that does not exist.')
assert(err:find('The library "varn-no-such-library" could not be loaded: ', 1, true), "The refusal should name the library, not: " .. tostring(err))
assert(#err > #'The library "varn-no-such-library" could not be loaded: ', "The refusal should give the reason of the system, not: " .. tostring(err))

print("The \"ffi\" load tests passed.")
