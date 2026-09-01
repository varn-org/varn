# 🔐 crypto

Hashing, keyed hashing, password storage, authenticated encryption, key derivation, encodings, UUIDs,
and cryptographically secure random bytes — backed by OpenSSL.

```lua
local crypto = require("crypto")
print(crypto.digest("SHA256", "varn"))
```

## Capabilities

| Function | What it does |
|---|---|
| `crypto.digest(algorithm, data, format?)` | Hash `data` with `"SHA256"`, `"SHA512"`, … — `format` is `"hex"` (default) or `"raw"`. |
| `crypto.hmac(algorithm, key, data, format?)` | Keyed hash (HMAC) with the same `format` options. |
| `crypto.randomBytes(n)` | `n` cryptographically secure random bytes. |
| `crypto.equals(a, b)` | Constant-time string comparison — use it to verify a MAC or token instead of `==`. |
| `crypto.base64Encode(data)` / `crypto.base64Decode(text)` | Standard base64 with padding. |
| `crypto.base64UrlEncode(data)` / `crypto.base64UrlDecode(text)` | URL-safe base64 (`-_`), no padding. |
| `crypto.hexEncode(data)` / `crypto.hexDecode(text)` | Lowercase hex; decoding rejects malformed input. |
| `crypto.uuidV4()` | A random RFC 4122 version 4 UUID string. |
| `crypto.uuidV7()` | A time-ordered RFC 4122 version 7 UUID string. |
| `crypto.hashPassword(password)` | A self-describing scrypt hash (`scrypt$N,r,p$salt$hash`). |
| `crypto.verifyPassword(password, hash)` | Constant-time password check; `false` (not an error) on mismatch. |
| `crypto.encrypt(key, plaintext)` | AES-256-GCM; returns `iv ‖ tag ‖ ciphertext` with a fresh random IV. The key is 32 bytes. |
| `crypto.decrypt(key, blob)` | AES-256-GCM decrypt; raises if the authentication tag does not verify. |
| `crypto.pbkdf2(password, salt, iterations, keyLen, algorithm?)` | A `keyLen`-byte key via PBKDF2-HMAC. |
| `crypto.hkdf(key, salt, info, keyLen, algorithm?)` | A `keyLen`-byte key via HKDF (extract + expand). |
| `crypto.rsaEncryptPublic(pemPublicKey, data)` | RSA encrypt `data` with a PEM public key using OAEP padding. |

## Availability

Native on every desktop and mobile platform. In the browser (wasm) the **essential subset** is
available — `digest`, `hmac`, `randomBytes`, `equals`, the base64/hex codecs, and the UUIDs — while
`encrypt`/`decrypt`, `hashPassword`/`verifyPassword`, `pbkdf2`, `hkdf`, and `rsaEncryptPublic` are native-only. See the
[platform matrix](../../docs/platform-availability.md).

## Reference, examples, and tests

- Full reference: [docs/lua-api/crypto.md](../../docs/lua-api/crypto.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
