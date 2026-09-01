-- hashes a string with SHA-512 and prints the start of the hex digest
local crypto = require("crypto")

local hex = crypto.digest("SHA512", "test", "hex")
assert(#hex == 128, "sha512 hex length")

print("crypto.digest SHA512 ok:", hex:sub(1, 16) .. "...")
