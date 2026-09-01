# 🔐 crypto

`crypto.digest(algo, data, format?)`, `crypto.hmac(algo, key, data, format?)`,
`crypto.randomBytes(n)` (OpenSSL-backed).

### Algorithm handling

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-001 | Weak hash MD5 | CWE-327 | MD5 used where collision-resistance matters |
| CRY-002 | Weak hash SHA1 | CWE-327 | SHA1 used for signatures/integrity |
| CRY-003 | Deprecated MD4/RIPEMD | CWE-327 | obsolete algorithms accepted |
| CRY-004 | Unknown algorithm | CWE-20 | `digest("bogus",…)` clear error, no crash |
| CRY-005 | Empty algorithm name | CWE-20 | `digest("",…)` rejected |
| CRY-006 | Algorithm whitespace/case | CWE-20 | `" sha256 "` trimmed/normalized |
| CRY-007 | Non-string algorithm | CWE-20 | number/table algorithm arg |
| CRY-008 | NUL in algorithm name | CWE-626 | `"sha\0256"` truncation |
| CRY-009 | Algorithm injection to OpenSSL | CWE-20 | crafted name passed to `EVP_*_fetch` |
| CRY-010 | XOF/variable-length digest | CWE-20 | SHAKE/variable-output handling |
| CRY-011 | HMAC with non-hash algo | CWE-20 | passing a cipher name to hmac |

### Input edges (digest/hmac)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-012 | Empty data | CWE-20 | `digest(algo,"")` correct empty-input hash |
| CRY-013 | Empty HMAC key | CWE-20 | zero-length key handling |
| CRY-014 | NUL in data | CWE-626 | binary data hashed fully (length-aware) |
| CRY-015 | NUL in key | CWE-626 | key bytes after NUL still used |
| CRY-016 | Huge data (no cap) | CWE-400 | large input is hashed; no artificial cap (trusted local code, like Node) |
| CRY-017 | Huge key (no cap) | CWE-400 | large key is accepted; no artificial cap |
| CRY-018 | Data at exactly the cap | CWE-193 | boundary at the size limit |
| CRY-019 | Non-string data/key | CWE-20 | number/table coercion |
| CRY-020 | High-byte / invalid-UTF8 input | CWE-176 | raw bytes hashed verbatim |
| CRY-021 | Known-answer vectors | CWE-327 | NIST test vectors match |
| CRY-022 | HMAC vector correctness | CWE-327 | RFC 4231 HMAC vectors |
| CRY-023 | Key-length cast (legacy HMAC) | CWE-190 | key/data near INT_MAX clamped |

### Output format & encoding

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-024 | hex format default | CWE-20 | default `format` is hex |
| CRY-025 | raw format binary-safe | CWE-626 | raw output preserves all bytes |
| CRY-026 | Invalid format string | CWE-20 | `format="b64"`/garbage rejected |
| CRY-027 | hex case/length | CWE-20 | lowercase, correct length for the algo |
| CRY-028 | raw length matches digest size | CWE-20 | output size equals the algorithm's |

### Randomness

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-029 | CSPRNG source | CWE-330 | `RAND_bytes`, not a PRNG |
| CRY-030 | RNG failure handling | CWE-703 | a `RAND_bytes` failure throws, never returns weak bytes |
| CRY-031 | Count zero | CWE-20 | `randomBytes(0)` returns empty |
| CRY-032 | Count negative | CWE-20 | negative count rejected |
| CRY-033 | Count upper bound | CWE-400 | no artificial cap; only a count past the int limit is rejected |
| CRY-034 | Count non-integer | CWE-20 | float/string count handling |
| CRY-035 | Distribution/bias | CWE-330 | output passes basic randomness checks |
| CRY-036 | Repeatability across forks | CWE-336 | RNG reseeds after fork |
| CRY-037 | Binary-safe output | CWE-626 | NUL bytes in random output preserved |
| CRY-038 | Integer overflow on count | CWE-190 | huge count wraps the size cast |

### Side-channels & misuse

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-039 | Non-constant-time compare | CWE-208 | comparing MACs with `==` leaks via timing |
| CRY-040 | Length leak in compare | CWE-208 | early-out on length difference |
| CRY-041 | Predictable randomness misuse | CWE-330 | `math.random` used for secrets instead |
| CRY-042 | Nonce/IV reuse | CWE-323 | same nonce reused in any AEAD usage |
| CRY-043 | Insufficient key length | CWE-326 | short keys for HMAC/derivation |
| CRY-044 | Hash used as MAC | CWE-328 | `digest(secret..msg)` length-extension |
| CRY-045 | Length-extension attack | CWE-328 | Merkle–Damgård extension via raw digest |
| CRY-046 | Missing salt | CWE-759 | unsalted hashing of low-entropy inputs |
| CRY-047 | Fast hash for passwords | CWE-916 | SHA/HMAC used where a KDF is required |
| CRY-048 | Padding oracle | CWE-696 | timing/error differences (if cipher ops added) |
| CRY-049 | Downgrade to weak algo | CWE-757 | caller coerced into MD5/SHA1 |
| CRY-050 | Key material in error/log | CWE-532 | key bytes leak on an error path |

### OpenSSL context & memory

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-051 | EVP ctx leak on init failure | CWE-401 | `EVP_MD_CTX` freed when init fails |
| CRY-052 | EVP ctx leak on update failure | CWE-401 | ctx freed on update error |
| CRY-053 | EVP ctx leak on final failure | CWE-401 | ctx freed on finalize error |
| CRY-054 | MAC ctx leak | CWE-401 | `EVP_MAC_CTX` freed on every path |
| CRY-055 | Fetched algo leak | CWE-401 | `EVP_MD_fetch`/`EVP_MAC_fetch` freed |
| CRY-056 | OOM during hashing | CWE-400 | allocation failure handled cleanly |
| CRY-057 | Output buffer bound | CWE-120 | `EVP_MAX_MD_SIZE` never exceeded |
| CRY-058 | MAC size validation | CWE-20 | zero/oversized MAC size rejected |
| CRY-059 | Uninitialized output read | CWE-457 | output length used before set |
| CRY-060 | Double-free on error unwinding | CWE-415 | ctx not freed twice across catch |

### Lua binding / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-061 | Exception across boundary | CWE-248 | OpenSSL errors caught at the binding |
| CRY-062 | Stack imbalance on error | CWE-664 | error path leaves a balanced Lua stack |
| CRY-063 | Argument count/type | CWE-20 | missing/extra/typed args |
| CRY-064 | NUL-truncated args | CWE-626 | length-aware reads for all args |
| CRY-065 | Concurrent digest calls | CWE-362 | no shared mutable global between calls |
| CRY-066 | Concurrent randomBytes | CWE-362 | RNG usable from worker threads |
| CRY-067 | Reentrant call via metatable | CWE-674 | re-entry during argument coercion |
| CRY-068 | Repeated calls leak | CWE-401 | tight loop does not leak memory |
| CRY-069 | Large output amplification | CWE-400 | hex output 2× input bound |
| CRY-070 | Dummy-driver parity | CWE-697 | non-OpenSSL build fails safe |
| CRY-071 | Algorithm fuzz | CWE-20 | random algorithm names never crash |
| CRY-072 | Data fuzz | CWE-20 | random/binary data never crashes |
| CRY-073 | Format fuzz | CWE-20 | random format args handled |
| CRY-074 | Count fuzz (randomBytes) | CWE-20 | random counts (incl. extremes) handled |
| CRY-075 | Streaming vs one-shot parity | CWE-697 | chunked vs single-buffer hashing agree |

### Higher-level crypto hygiene (where the app builds on these)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-076 | Token entropy | CWE-331 | derived tokens use ≥128 bits |
| CRY-077 | Predictable IDs | CWE-330 | IDs built from time/counter not RNG |
| CRY-078 | HMAC truncation | CWE-347 | truncated MAC comparison |
| CRY-079 | Key reuse across purposes | CWE-323 | one key for sign + encrypt |
| CRY-080 | Hardcoded/default secret | CWE-798 | shipped default key |
| CRY-081 | Secret in source/repo | CWE-540 | key committed |
| CRY-082 | Insufficient KDF iterations | CWE-916 | weak password stretching |
| CRY-083 | Missing domain separation | CWE-327 | same hash input across contexts |
| CRY-084 | Signature malleability | CWE-347 | alternate encodings verify |
| CRY-085 | Cross-algorithm confusion | CWE-327 | algo agility lets a weaker one in |
| CRY-086 | Timing of HMAC verify | CWE-208 | verify path not constant-time |
| CRY-087 | Random reuse within a window | CWE-330 | cached randomness across requests |
| CRY-088 | Error oracle on verify | CWE-203 | distinct errors reveal which check failed |
| CRY-089 | Endianness in digest framing | CWE-188 | length framing differs by platform |
| CRY-090 | Locale-affected hex output | CWE-697 | locale changes hex formatting |
| CRY-091 | Thread-unsafe OpenSSL init | CWE-362 | first-use init races |
| CRY-092 | FIPS/provider availability | CWE-703 | missing provider handled gracefully |
| CRY-093 | Algorithm allow-list bypass | CWE-693 | only-approved-algos policy evaded |
| CRY-094 | Output reused as key | CWE-327 | digest output fed back as a key |
| CRY-095 | HMAC key shorter than block | CWE-326 | very short keys |
| CRY-096 | HMAC key longer than block | CWE-20 | over-block keys are hashed first (correctness) |
| CRY-097 | Constant-time hex compare | CWE-208 | hex-string comparison timing |
| CRY-098 | Memory zeroization | CWE-244 | secrets not wiped after use |
| CRY-099 | Error-path info leak | CWE-209 | error text reveals algorithm internals |
| CRY-100 | Fuzz all three entry points | CWE-20 | combined random inputs never crash |

---

## Additional cases (documented attacks & CVEs)

### Known crypto attacks (named)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-101 | Bleichenbacher RSA PKCS#1v1.5 | CWE-203 | padding oracle decrypts RSA (consumer ext) |
| CRY-102 | ROBOT (Bleichenbacher revival) | CVE-2017-13099 | TLS RSA oracle |
| CRY-103 | Padding oracle (Vaudenay) | CWE-696 | CBC padding oracle decrypts |
| CRY-104 | Lucky13 | CVE-2013-0169 | CBC-MAC timing oracle |
| CRY-105 | POODLE | CVE-2014-3566 | SSLv3 CBC padding |
| CRY-106 | Sweet32 | CVE-2016-2183 | 64-bit block birthday attack (3DES/Blowfish) |
| CRY-107 | FREAK | CVE-2015-0204 | export-grade RSA downgrade |
| CRY-108 | Logjam | CVE-2015-4000 | weak DH parameter downgrade |
| CRY-109 | Raccoon | CVE-2020-1968 | DH timing side-channel |
| CRY-110 | DROWN | CVE-2016-0800 | SSLv2 cross-protocol RSA |
| CRY-111 | ROCA weak key | CVE-2017-15361 | factorable Infineon RSA keys |
| CRY-112 | Invalid-curve attack | CWE-327 | off-curve EC point leaks the key |
| CRY-113 | Small-subgroup / weak DH | CWE-327 | non-safe-prime DH params |
| CRY-114 | ECDSA nonce reuse (PS3) | CWE-323 | repeated `k` recovers the private key |
| CRY-115 | ECDSA biased nonce | CWE-338 | lattice attack on biased `k` |
| CRY-116 | RSA-CRT fault attack | CWE-320 | a fault during CRT leaks `p`/`q` |
| CRY-117 | Psychic signature | CVE-2022-21449 | ECDSA `(r,s)=(0,0)` accepted |
| CRY-118 | Hash length extension | CWE-328 | extend a `H(secret‖msg)` MAC |
| CRY-119 | MD5 chosen-prefix collision | CWE-327 | forged certificates (Flame) |
| CRY-120 | SHA-1 SHAttered collision | CWE-327 | practical SHA-1 collision |
| CRY-121 | Nostradamus / herding | CWE-327 | chosen-target-prefix collisions |
| CRY-122 | CBC IV predictability (BEAST) | CVE-2011-3389 | predictable IV chosen-plaintext |
| CRY-123 | ECB pattern leakage | CWE-327 | identical blocks reveal structure |
| CRY-124 | GCM nonce reuse forgery | CWE-323 | repeated nonce → key/forgery |
| CRY-125 | Key/IV reuse across messages | CWE-323 | stream-cipher keystream reuse |

### RNG / key generation (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-126 | Debian OpenSSL weak keys | CVE-2008-0166 | tiny keyspace from a broken RNG |
| CRY-127 | Dual_EC_DRBG backdoor | CWE-1240 | predictable DRBG output |
| CRY-128 | Insufficient entropy at boot | CWE-331 | early-boot RNG predictability |
| CRY-129 | RNG state not reseeded after fork | CWE-336 | parent/child share RNG state |
| CRY-130 | VM snapshot RNG reuse | CWE-330 | restored snapshot replays randomness |
| CRY-131 | PID/time-seeded PRNG | CWE-337 | predictable seed |
| CRY-132 | `math.random` for secrets | CWE-338 | non-CSPRNG used for tokens |
| CRY-133 | RNG failure silently ignored | CWE-703 | a failed draw returns weak/zero bytes |
| CRY-134 | Short token entropy | CWE-331 | < 128-bit token |
| CRY-135 | Predictable IV/salt | CWE-329 | counter/time-based IV |
| CRY-136 | Salt reuse | CWE-759 | shared salt across hashes |
| CRY-137 | Nonce counter overflow | CWE-190 | counter wraps and repeats |

### Timing / side-channel (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-138 | Non-constant-time MAC compare | CWE-208 | byte-wise compare leaks the MAC |
| CRY-139 | Length-leak in compare | CWE-208 | early length-mismatch exit |
| CRY-140 | Cache-timing AES (T-tables) | CWE-385 | data-dependent table lookups |
| CRY-141 | RSA timing (Kocher) | CWE-385 | modular-exponentiation timing |
| CRY-142 | Branch-on-secret | CWE-1300 | secret-dependent branch |
| CRY-143 | Error-message oracle | CWE-203 | distinct errors reveal the failing check |
| CRY-144 | Compression+crypto (CRIME/BREACH) | CVE-2012-4929 | length oracle on compressed secrets |
| CRY-145 | Power/EM side-channel | CWE-1300 | hardware leakage (out of scope, note) |
| CRY-146 | Memory not zeroized | CWE-244 | secrets linger after use |
| CRY-147 | Swap/core-dump key leak | CWE-528 | keys paged to disk / dumped |

### Misuse / protocol (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-148 | Fast hash for passwords | CWE-916 | SHA/HMAC instead of a KDF |
| CRY-149 | Missing KDF stretching | CWE-916 | weak password storage |
| CRY-150 | Encrypt-and-MAC ordering | CWE-649 | wrong AE composition |
| CRY-151 | MAC-then-encrypt | CWE-649 | unauthenticated decryption |
| CRY-152 | Unauthenticated encryption | CWE-353 | no integrity on ciphertext |
| CRY-153 | Key reuse across purposes | CWE-323 | one key for sign and encrypt |
| CRY-154 | Algorithm confusion (agility) | CWE-757 | downgrade to a weaker algorithm |
| CRY-155 | Truncated MAC acceptance | CWE-347 | comparing a prefix of the MAC |
| CRY-156 | Signature malleability | CWE-347 | alternate encodings verify |
| CRY-157 | Cross-protocol key reuse | CWE-323 | same key in two protocols |
| CRY-158 | Hardcoded/default key | CWE-798 | shipped secret |
| CRY-159 | Key committed to repo | CWE-540 | secret in source control |
| CRY-160 | Insecure key transport | CWE-319 | key sent in cleartext |
| CRY-161 | Weak password policy upstream | CWE-521 | trivial secrets accepted |
| CRY-162 | No domain separation | CWE-327 | same hash input across contexts |
| CRY-163 | Deterministic encryption leak | CWE-327 | equal plaintext → equal ciphertext |
| CRY-164 | Missing AAD binding | CWE-345 | context not authenticated |

### Input / library / build (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-165 | Heartbleed (lib) | CVE-2014-0160 | OpenSSL memory disclosure |
| CRY-166 | OpenSSL infinite loop | CVE-2022-0778 | BN_mod_sqrt parse DoS |
| CRY-167 | OpenSSL punycode overflow | CVE-2022-3602 | X.509 name buffer overflow |
| CRY-168 | Unpatched provider CVE | CWE-1395 | stale crypto backend |
| CRY-169 | Algorithm name injection | CWE-20 | crafted name to `EVP_*_fetch` |
| CRY-170 | NUL in algorithm name | CWE-626 | name truncation |
| CRY-171 | Large input (no cap) | CWE-193 | multi-megabyte input hashes fine; no artificial cap |
| CRY-172 | Large key (no cap) | CWE-193 | large key accepted; no artificial cap |
| CRY-173 | randomBytes large count | CWE-400 | a large in-range count is honored (no million-byte cap) |
| CRY-174 | Negative/zero count | CWE-20 | `randomBytes(-1)` rejected, `(0)` returns empty |
| CRY-175 | Non-integer count | CWE-20 | float/string count |
| CRY-176 | Integer overflow in count cast | CWE-190 | a count past the int limit is rejected, never wrapped |
| CRY-177 | Large HMAC key (no cap) | CWE-400 | large key accepted; no artificial cap |
| CRY-178 | XOF length request | CWE-20 | SHAKE variable output |
| CRY-179 | Unknown digest name | CWE-20 | clean error, no crash |
| CRY-180 | Empty algorithm | CWE-20 | rejected |

### Memory / binding / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-181 | EVP_MD_CTX leak on init fail | CWE-401 | ctx freed on every error |
| CRY-182 | EVP_MD_CTX leak on update fail | CWE-401 | ctx freed |
| CRY-183 | EVP_MAC_CTX leak | CWE-401 | MAC ctx freed |
| CRY-184 | Fetched algo leak | CWE-401 | `EVP_*_fetch` result freed |
| CRY-185 | Output buffer bound | CWE-120 | never exceed `EVP_MAX_MD_SIZE` |
| CRY-186 | Uninitialized out-length read | CWE-457 | output length used before set |
| CRY-187 | Double-free on unwinding | CWE-415 | ctx not freed twice across catch |
| CRY-188 | Exception across boundary | CWE-248 | OpenSSL errors caught |
| CRY-189 | Stack imbalance on error | CWE-664 | error path leaves a balanced Lua stack |
| CRY-190 | NUL-truncated data/key | CWE-626 | length-aware reads |
| CRY-191 | Concurrent digest calls | CWE-362 | no shared mutable state |
| CRY-192 | Concurrent randomBytes | CWE-362 | RNG usable from workers |
| CRY-193 | First-use init race | CWE-362 | OpenSSL init concurrency |
| CRY-194 | Reentrant call via metatable | CWE-674 | re-entry during arg coercion |
| CRY-195 | Repeated-call leak | CWE-401 | tight loop does not leak |
| CRY-196 | Hex output amplification | CWE-400 | 2× input size bound |
| CRY-197 | ASan trip in hashing | CWE-125 | OOB read on malformed input |
| CRY-198 | Differential vs reference | CWE-697 | output mismatch with a known impl |
| CRY-199 | Streaming vs one-shot parity | CWE-697 | chunked == single-buffer |
| CRY-200 | Wycheproof test vectors | CWE-327 | run Google Wycheproof corpus |

---

## Round 3 — deeper / documented

### Named attacks (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-201 | SLOTH (RSA-MD5 sig) | CVE-2015-7575 | transcript collision on weak hash |
| CRY-202 | Bleichenbacher variants (ROBOT) | CVE-2017-13099 | RSA padding oracle revival |
| CRY-203 | Manger OAEP oracle | CWE-203 | OAEP decryption oracle |
| CRY-204 | Cache-bleed | CWE-385 | cache-bank timing on RSA |
| CRY-205 | Flush+Reload | CWE-385 | shared-cache key recovery |
| CRY-206 | Prime+Probe | CWE-385 | cache-set timing |
| CRY-207 | Hertzbleed | CWE-385 | frequency-scaling side-channel |
| CRY-208 | Minerva (ECDSA timing) | CWE-385 | nonce-length timing leak |
| CRY-209 | TPM-Fail | CVE-2019-16863 | ECDSA timing on TPM |
| CRY-210 | LadderLeak | CWE-385 | tiny nonce bias breaks ECDSA |
| CRY-211 | Fault-injection on verify | CWE-320 | skip the signature check |
| CRY-212 | GCM short-tag forgery | CWE-354 | truncated auth tag |
| CRY-213 | XSalsa/ChaCha nonce reuse | CWE-323 | keystream reuse |
| CRY-214 | CBC bit-flipping | CWE-353 | predictable plaintext mauling |
| CRY-215 | Key-commitment failure (AEAD) | CWE-310 | one ciphertext decrypts under two keys |
| CRY-216 | Invalid-curve / twist attack | CWE-327 | off-curve point |
| CRY-217 | Small-subgroup confinement | CWE-327 | non-prime-order subgroup |
| CRY-218 | DHE parameter validation | CWE-327 | unvalidated group |
| CRY-219 | RSA key < 2048 | CWE-326 | factorable modulus |
| CRY-220 | Shared RSA modulus | CWE-326 | common-factor key recovery |

### RNG / entropy (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-221 | NetBSD/OpenBSD RNG bugs (class) | CWE-330 | historical weak seeding |
| CRY-222 | Android SecureRandom (2013) | CWE-330 | predictable keys (class) |
| CRY-223 | rand()/PRNG for secrets | CWE-338 | non-CSPRNG token |
| CRY-224 | Time-based seed | CWE-337 | predictable seed |
| CRY-225 | Counter as a nonce without reseed | CWE-323 | nonce repetition |
| CRY-226 | RNG reuse after VM clone | CWE-330 | snapshot replays randomness |
| CRY-227 | Fork without reseed | CWE-336 | parent/child shared state |
| CRY-228 | Insufficient seed entropy | CWE-332 | low-entropy seed |
| CRY-229 | RNG output cached | CWE-330 | reused across requests |
| CRY-230 | Predictable IV from counter | CWE-329 | IV reuse/predictability |

### Protocol / PKI / KDF (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-231 | HKDF info/salt misuse | CWE-327 | missing context separation |
| CRY-232 | PBKDF2 low iterations | CWE-916 | weak stretching |
| CRY-233 | bcrypt 72-byte truncation | CWE-916 | long passwords truncated |
| CRY-234 | scrypt/Argon2 params too low | CWE-916 | cheap to brute-force |
| CRY-235 | Pepper vs salt confusion | CWE-759 | missing server-side pepper |
| CRY-236 | Certificate chain not validated | CWE-295 | unverified chain |
| CRY-237 | Name-constraints ignored | CWE-295 | sub-CA over-issuance |
| CRY-238 | EKU not checked | CWE-295 | wrong key-usage accepted |
| CRY-239 | Revocation not checked | CWE-299 | revoked cert accepted |
| CRY-240 | Pinning absent | CWE-295 | no certificate pinning |
| CRY-241 | Algorithm allow-list bypass | CWE-757 | downgrade to a weak algo |
| CRY-242 | Cross-algorithm key reuse | CWE-323 | same key, two algorithms |
| CRY-243 | Signature without context tag | CWE-345 | cross-context signature reuse |
| CRY-244 | Detached-signature confusion | CWE-347 | wrong data signed |
| CRY-245 | Post-quantum readiness | CWE-327 | no migration path (note) |

### Side-channel / memory hygiene (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-246 | Branch-on-secret in compare | CWE-1300 | data-dependent branch |
| CRY-247 | Table-lookup on secret index | CWE-1300 | cache-line leak |
| CRY-248 | Variable-time bignum | CWE-385 | modexp timing |
| CRY-249 | Non-CT conditional swap | CWE-385 | secret-dependent swap |
| CRY-250 | Secret in swap/coredump | CWE-528 | key paged to disk |
| CRY-251 | Memory not zeroized | CWE-244 | key lingers after use |
| CRY-252 | Compiler removes secure-wipe | CWE-14 | optimizer drops the memset |
| CRY-253 | Secret in exception text | CWE-209 | key bytes in an error |
| CRY-254 | Secret logged | CWE-532 | key written to a log |
| CRY-255 | Timing of length check | CWE-208 | early-out on length |

### Input / OpenSSL / build (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-256 | OpenSSL X.509 GENERAL_NAME (CVE-2023-0286) | CWE-843 | type confusion in cert parsing |
| CRY-257 | OpenSSL infinite loop (CVE-2022-0778) | CWE-835 | BN_mod_sqrt DoS |
| CRY-258 | OpenSSL punycode overflow (CVE-2022-3602) | CWE-787 | name-constraint buffer overflow |
| CRY-259 | Provider not loaded | CWE-703 | algorithm fetch fails gracefully |
| CRY-260 | FIPS mode unavailable | CWE-703 | approved-only policy handled |
| CRY-261 | EVP fetch with attacker name | CWE-20 | crafted algorithm string |
| CRY-262 | Algorithm name NUL truncation | CWE-626 | name cut at NUL |
| CRY-263 | XOF length request | CWE-20 | SHAKE variable output |
| CRY-264 | HMAC key > block hashed first | CWE-20 | over-block key correctness |
| CRY-265 | HMAC key shorter than output | CWE-326 | weak key length |
| CRY-266 | Large input (no cap) | CWE-193 | large input hashes fine; no artificial cap |
| CRY-267 | randomBytes large count | CWE-193 | a 1,000,000-byte request is honored (no artificial cap) |
| CRY-268 | Count integer overflow | CWE-190 | huge count wraps |
| CRY-269 | Streaming vs one-shot parity | CWE-697 | chunked == single-buffer |
| CRY-270 | Differential vs reference impl | CWE-697 | output mismatch |

### Binding / concurrency / leak (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-271 | EVP ctx leak on init fail | CWE-401 | ctx freed |
| CRY-272 | EVP ctx leak on update fail | CWE-401 | ctx freed |
| CRY-273 | EVP ctx leak on final fail | CWE-401 | ctx freed |
| CRY-274 | MAC ctx leak | CWE-401 | MAC ctx freed |
| CRY-275 | Fetched algo not freed | CWE-401 | fetch result freed |
| CRY-276 | Double-free on unwind | CWE-415 | not freed twice across catch |
| CRY-277 | Output buffer bound | CWE-120 | never exceed max md size |
| CRY-278 | Uninitialized out-length | CWE-457 | length used before set |
| CRY-279 | OOM during hashing | CWE-400 | allocation failure handled |
| CRY-280 | Exception across boundary | CWE-248 | OpenSSL errors caught |
| CRY-281 | Stack imbalance on error | CWE-664 | error path balanced |
| CRY-282 | NUL-truncated args | CWE-626 | length-aware reads |
| CRY-283 | Concurrent digest calls | CWE-362 | no shared mutable global |
| CRY-284 | Concurrent randomBytes | CWE-362 | RNG usable from workers |
| CRY-285 | First-use init race | CWE-362 | OpenSSL init concurrency |
| CRY-286 | Reentrant call via metatable | CWE-674 | re-entry during coercion |
| CRY-287 | Repeated-call leak | CWE-401 | tight loop no leak |
| CRY-288 | Hex output amplification | CWE-400 | 2× input bound |
| CRY-289 | ASan trip in hashing | CWE-125 | OOB on malformed input |
| CRY-290 | Dummy-driver parity | CWE-697 | non-OpenSSL build fails safe |

### Higher-level hygiene (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| CRY-291 | Hardcoded default key | CWE-798 | shipped secret |
| CRY-292 | Secret committed to repo | CWE-540 | key in source control |
| CRY-293 | Insecure key transport | CWE-319 | key sent in cleartext |
| CRY-294 | Token entropy < 128 bits | CWE-331 | guessable token |
| CRY-295 | Predictable ID generation | CWE-330 | time/counter-based id |
| CRY-296 | Fast hash for passwords | CWE-916 | KDF required |
| CRY-297 | Unauthenticated encryption | CWE-353 | no integrity check |
| CRY-298 | Deterministic encryption leak | CWE-327 | equal plaintext leaks |
| CRY-299 | Missing AAD binding | CWE-345 | context not authenticated |
| CRY-300 | Fuzz all three entry points | CWE-20 | random algo/data/format/count never crash |
