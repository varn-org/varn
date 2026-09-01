# 🔐 crypto

Hashing, keyed hashing, and cryptographically secure random bytes.

- `crypto.digest(algorithm, data, format?)` → a digest of `data`. `algorithm` is a name like `"SHA256"` or `"SHA512"`. `format` is `"hex"` (default) or `"raw"`.
- `crypto.hmac(algorithm, key, data, format?)` → an HMAC, with the same `format` options.
- `crypto.randomBytes(n)` → `n` random bytes as a string.
- `crypto.equals(a, b)` → `true` if the two strings are equal, compared in constant time. Use this to verify a MAC, token, or any secret-derived value instead of `==`, which leaks bytes through timing.

## Codecs

Binary-safe, length-aware encoders and decoders. Decoding rejects malformed input with an error.

- `crypto.base64Encode(data)` / `crypto.base64Decode(text)` → standard base64 with padding (`+/`, `=`).
- `crypto.base64UrlEncode(data)` / `crypto.base64UrlDecode(text)` → URL-safe base64 (`-_`) with **no** padding. Decoding accepts either alphabet and optional padding.
- `crypto.hexEncode(data)` / `crypto.hexDecode(text)` → lowercase hex. Decoding accepts upper or lower case and rejects odd-length or non-hex input.

## Identifiers

- `crypto.uuidV4()` → an RFC 4122 version 4 (random) UUID string, e.g. `"f47ac10b-58cc-4372-a567-0e02b2c3d479"`.
- `crypto.uuidV7()` → an RFC 4122 version 7 (time-ordered) UUID string. The first 48 bits are a Unix-millisecond timestamp, so the values sort by creation time.

## Passwords

- `crypto.hashPassword(password)` → a self-describing hash string of the form `scrypt$N,r,p$salt$hash` (salt and hash are URL-safe base64). Uses OpenSSL scrypt with a fresh random salt and interactive cost parameters (N=2^15, r=8, p=1). Store the returned string as-is.
- `crypto.verifyPassword(password, hash)` → `true` if `password` matches the stored `hash`, compared in constant time. Returns `false` (not an error) for a wrong password or a malformed hash string.

## Authenticated encryption

AES-256-GCM via OpenSSL. The key must be exactly 32 bytes (derive one with `crypto.pbkdf2`/`crypto.hkdf` or use `crypto.randomBytes(32)`).

- `crypto.encrypt(key, plaintext)` → a binary blob laid out as `iv(12) || tag(16) || ciphertext`. A fresh random IV is generated per call.
- `crypto.decrypt(key, blob)` → the plaintext, or an **error** if the authentication tag does not verify (wrong key or tampered blob).
- `crypto.rsaEncryptPublic(pemPublicKey, data)` → `data` encrypted with a PEM-encoded RSA public key using OAEP padding, returned as a binary blob.

## Key derivation

Both return raw key bytes. `algo` is optional and defaults to `"SHA256"`.

- `crypto.pbkdf2(password, salt, iterations, keyLen, algo?)` → a `keyLen`-byte key derived with PBKDF2-HMAC.
- `crypto.hkdf(key, salt, info, keyLen, algo?)` → a `keyLen`-byte key derived with HKDF (extract + expand).

There is no artificial input-size limit — Varn runs your own code on your own machine, so hashing or generating large data is allowed (bounded only by memory and the platform's integer limits). The digest, hmac, randomBytes, codecs, and UUID helpers work in the browser build; the authenticated-encryption, password-hashing, key-derivation, and RSA functions are native-only and reject with "not available in this build" under wasm.

## Examples

### `digest_raw.lua`

```lua
-- checks the raw digest length for a fixed input string
local crypto = require("crypto")

local raw = crypto.digest("SHA256", "raw-test", "raw")
assert(#raw == 32, "raw sha256 length")
print("crypto.digest raw ok, len=", #raw)
```

### `digest_sha256.lua`

```lua
-- checks a short string against a known hex fingerprint
local crypto = require("crypto")

local h = crypto.digest("SHA256", "varn", "hex")
assert(#h == 64, "sha256 hex length")
assert(h:match("^[0-9a-f]+$"), "expected lowercase hex")
print("crypto.digest SHA256 hex ok:", h:sub(1, 16), "...")
```

### `digest_sha512.lua`

```lua
-- hashes a string with SHA-512 and prints the start of the hex digest.
local crypto = require("crypto")

local hex = crypto.digest("SHA512", "test", "hex")
assert(#hex == 128, "sha512 hex length")

print("crypto.digest SHA512 ok:", hex:sub(1, 16) .. "...")
```

### `digest_vectors.lua`

```lua
-- verifies SHA-256 against known vectors for the empty and "abc" inputs.
local crypto = require("crypto")

assert(crypto.digest("SHA256", "", "hex")
    == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA256 empty string")
assert(crypto.digest("SHA256", "abc", "hex")
    == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 abc")

print("crypto.digest SHA256 vectors ok (empty, abc)")
```

### `hmac_vectors.lua`

```lua
-- verifies HMAC-SHA256 against RFC 4231 test case 1.
local crypto = require("crypto")

local key = string.rep("\x0b", 20)
local mac = crypto.hmac("SHA256", key, "Hi There", "hex")
assert(mac == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "HMAC-SHA256 RFC 4231 case 1")

print("crypto.hmac RFC 4231 vector ok")
```

### `random_bytes.lua`

```lua
-- asserts only on the length fields returned for random byte requests
local crypto = require("crypto")

local b16 = crypto.randomBytes(16)
assert(#b16 == 16, "length")
local b32 = crypto.randomBytes(32)
assert(#b32 == 32, "length")
print("crypto.randomBytes ok")
```

### `crypto_extra.lua`

```lua
-- codecs, uuids, password hashing, authenticated encryption, and key derivation.
local crypto = require("crypto")

-- codecs round-trip arbitrary binary data.
assert(crypto.base64Decode(crypto.base64Encode("foo\0bar")) == "foo\0bar")
assert(crypto.base64UrlDecode(crypto.base64UrlEncode("\251\255\190")) == "\251\255\190")
assert(crypto.hexDecode(crypto.hexEncode("a\0b")) == "a\0b")

-- identifiers.
print("uuid v4:", crypto.uuidV4())
print("uuid v7:", crypto.uuidV7())

-- password storage.
local stored = crypto.hashPassword("hunter2")
assert(crypto.verifyPassword("hunter2", stored))
assert(not crypto.verifyPassword("nope", stored))

-- authenticated encryption.
local key = crypto.randomBytes(32)
local blob = crypto.encrypt(key, "attack at dawn")
assert(crypto.decrypt(key, blob) == "attack at dawn")

-- key derivation.
local k1 = crypto.pbkdf2("password", "salt", 1, 32, "SHA256")
assert(#k1 == 32)
local k2 = crypto.hkdf(key, "", "session", 32, "SHA256")
assert(#k2 == 32)

print("crypto extra example ok")
```

## Under the hood

Hashes, HMAC, random bytes, base64/hex codecs, UUIDs, scrypt password hashing, AES-256-GCM, and PBKDF2/HKDF are all provided by OpenSSL.
