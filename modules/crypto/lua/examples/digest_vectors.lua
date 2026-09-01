-- verifies SHA-256 against known vectors for the empty and "abc" inputs
local crypto = require("crypto")

assert(crypto.digest("SHA256", "", "hex")
    == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA256 empty string")
assert(crypto.digest("SHA256", "abc", "hex")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 abc")

print("crypto.digest SHA256 vectors ok (empty, abc)")
