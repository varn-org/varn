-- demonstrates the codec, uuid, password, encryption, and key derivation helpers
local crypto = require("crypto")

-- base64 and base64url codecs round-trip binary data
local data = "varn \240\159\148\144 crypto"
print("base64:    ", crypto.base64Encode(data))
print("base64url: ", crypto.base64UrlEncode(data))
print("hex:       ", crypto.hexEncode(data))
assert(crypto.base64Decode(crypto.base64Encode(data)) == data)
assert(crypto.base64UrlDecode(crypto.base64UrlEncode(data)) == data)
assert(crypto.hexDecode(crypto.hexEncode(data)) == data)

-- random identifiers, time-ordered with uuidV7
print("uuid v4:   ", crypto.uuidV4())
print("uuid v7:   ", crypto.uuidV7())

-- password hashing produces a self-describing string you can store as-is
local stored = crypto.hashPassword("hunter2")
print("password:  ", stored)
assert(crypto.verifyPassword("hunter2", stored))
assert(not crypto.verifyPassword("nope", stored))

-- authenticated encryption with a 32-byte key, tamper-evident on decrypt
local key = crypto.randomBytes(32)
local blob = crypto.encrypt(key, "attack at dawn")
print("ciphertext bytes:", #blob)
assert(crypto.decrypt(key, blob) == "attack at dawn")

-- key derivation from a password or a high-entropy key
local derived = crypto.pbkdf2("hunter2", crypto.randomBytes(16), 100000, 32, "SHA256")
print("pbkdf2 key bytes:", #derived)

local sub = crypto.hkdf(key, "", "session", 32, "SHA256")
print("hkdf key bytes:  ", #sub)

print("crypto extra example ok")
