# 🧾 json

The calls `json.encode(value [,{pretty,indent}])` / `json.decode(text)` and aliases are covered here. Full Lua↔JSON type conversion backed by nlohmann.

### Encode — type conversion & edges

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-001 | Sequence → array | CWE-704 | The table `{1,2,3}` must encode as `[1,2,3]` not an object |
| JSON-002 | Map → object | CWE-704 | String-keyed table encodes as `{}` |
| JSON-003 | Empty table ambiguity | CWE-704 | The table `{}` → object vs array convention |
| JSON-004 | Sparse array | CWE-704 | The table `{[1]=1,[3]=3}` (gap) → object not truncated array |
| JSON-005 | Mixed keys | CWE-704 | Classification of `{[1]=1, x=2}` |
| JSON-006 | 1-based vs 0-based | CWE-704 | Handling of `{[0]=1}` |
| JSON-007 | Float-integer key alias | CWE-704 | The table `{[1.0]=…}` vs `{[1]=…}` |
| JSON-008 | NaN → `null` | CWE-248 | The value `0/0` must encode as `null`, never throw |
| JSON-009 | +Inf → `null` | CWE-248 | The value `1/0` |
| JSON-010 | -Inf → `null` | CWE-248 | The value `-1/0` |
| JSON-011 | Negative zero | CWE-704 | A `-0.0` round-trip |
| JSON-012 | Very large integer | CWE-681 | Precision/wrap around `2^63` |
| JSON-013 | Very large float | CWE-681 | Values like `1e308`, subnormals |
| JSON-014 | Integer vs float emit | CWE-704 | The `3` vs `3.0` distinction |
| JSON-015 | Boolean / `nil` scalar | CWE-704 | The scalars `true`/`false`/`nil`→`null` |
| JSON-016 | Top-level scalar | CWE-20 | Encode a bare string/number/bool |
| JSON-017 | Invalid UTF-8 in value | CWE-176 | The bytes `\xff\xfe` replaced, no throw across boundary |
| JSON-018 | Invalid UTF-8 in key | CWE-176 | Raw bytes as a key |
| JSON-019 | Embedded NUL in string | CWE-626 | The string `"a\0b"` → `\u0000` round-trip preserves length |
| JSON-020 | Control chars escaped | CWE-116 | Bytes `\x01`-`\x1f` emitted as `\uXXXX` |
| JSON-021 | Quote/backslash/slash escaping | CWE-116 | The characters `"`, `\`, `/` correctly escaped |
| JSON-022 | Unicode surrogate handling | CWE-176 | Astral chars / lone surrogates |
| JSON-023 | Non-string/number key | CWE-704 | Boolean/table key coercion |
| JSON-024 | Function/userdata value | CWE-20 | Unsupported value handling |
| JSON-025 | Metatable `__tostring` not abused | CWE-913 | Encoding ignores metamethods |
| JSON-026 | Encode mutates input | CWE-664 | Input table not modified during encode |

### Encode — recursion, memory, options

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-027 | Deep nesting encode | CWE-674 | Depth cap engages, no stack overflow |
| JSON-028 | Self-referential table | CWE-674 | Cyclic table does not infinite-loop |
| JSON-029 | Wide table | CWE-400 | A million keys bounded |
| JSON-030 | Huge string value | CWE-400 | Multi-GB string memory |
| JSON-031 | Output amplification | CWE-409 | Small input → huge escaped output |
| JSON-032 | `lua_checkstack` on deep encode | CWE-674 | Stack reservation before pushes |
| JSON-033 | Pretty indent huge | CWE-400 | An `indent = 2^31` over-allocates padding |
| JSON-034 | Pretty indent negative | CWE-20 | Negative indent handling |
| JSON-035 | Pretty re-parse type drift | CWE-704 | Pretty path preserves int/float types |
| JSON-036 | Options type confusion | CWE-20 | Non-table/garbage options |

### Decode — parser robustness & fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-037 | Deep nesting parse | CWE-674 | A run of `[`×N rejected before overflow |
| JSON-038 | Pre-scan vs parser depth mismatch | CWE-674 | Depth guard matches the real parser limit |
| JSON-039 | Empty input | CWE-20 | The input `""` rejected |
| JSON-040 | Whitespace-only | CWE-20 | The input `"   "` rejected |
| JSON-041 | Truncated object | CWE-20 | The input `{"a":` rejected |
| JSON-042 | Truncated array | CWE-20 | The input `[1,2,` rejected |
| JSON-043 | Trailing garbage | CWE-20 | The input `{}x` rejected |
| JSON-044 | Trailing comma | CWE-20 | The input `[1,2,]` per strict mode |
| JSON-045 | Leading zero / bad number | CWE-20 | Numbers like `01`, `1.`, `.5`, `1e` |
| JSON-046 | Bare control bytes | CWE-74 | Raw control char inside a string |
| JSON-047 | Bad escape sequence | CWE-20 | Escapes like `"\x"`, `"\uZZZZ"` |
| JSON-048 | Lone surrogate in input | CWE-176 | The string `"\ud800"` without pair |
| JSON-049 | Duplicate keys | CWE-20 | The object `{"a":1,"a":2}` deterministic last-wins |
| JSON-050 | Huge number literal | CWE-400 | The `1e1000000` exponent |
| JSON-051 | Number precision loss | CWE-681 | The number `9999999999999999` |
| JSON-052 | Unicode BOM prefix | CWE-20 | Leading BOM handling |
| JSON-053 | NaN/Infinity literals | CWE-20 | Non-standard `NaN`/`Infinity` tokens |
| JSON-054 | Comments in input | CWE-20 | Comments `//`/`/* */` rejected (strict) |
| JSON-055 | Single quotes | CWE-20 | The input `'x'` rejected |
| JSON-056 | Unquoted keys | CWE-20 | The input `{a:1}` rejected |
| JSON-057 | Object key collision via unicode | CWE-178 | Normalized-equal distinct keys |
| JSON-058 | Very long string | CWE-400 | Multi-GB string value |
| JSON-059 | Many small tokens | CWE-407 | Parse-time complexity of `[1,1,1,…]` |
| JSON-060 | NUL in input | CWE-626 | Embedded NUL in the document |
| JSON-061 | Non-UTF8 document | CWE-176 | Invalid encoding rejected/handled |
| JSON-062 | Mixed array/object close | CWE-20 | Mismatched brackets in `[1}` |
| JSON-063 | Unbalanced quotes | CWE-20 | The input `"abc` |
| JSON-064 | Escaped-quote edge | CWE-20 | The trailing escape in `"a\\"` |

### Decode — type mapping back to Lua

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-065 | `null` → `nil` semantics | CWE-704 | The object `{"a":null}` drops the key (documented) |
| JSON-066 | Array becomes 1-based table | CWE-704 | The array `[10,20]` → `t[1]=10` |
| JSON-067 | Unsigned > int max | CWE-681 | Large unsigned mapped to number not wrapped int |
| JSON-068 | Integer vs float on decode | CWE-704 | The value `3` int vs `3.0` float in Lua |
| JSON-069 | Deeply nested push | CWE-674 | Push depth bounded with `lua_checkstack` |
| JSON-070 | Object key with NUL | CWE-626 | Key bytes preserved via `pushlstring` |
| JSON-071 | Empty array vs object decode | CWE-704 | The inputs `[]` vs `{}` distinct Lua shapes |
| JSON-072 | Numeric string key | CWE-704 | The object `{"1":"v"}` → string key not array |

### Memory / boundary / concurrency

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-073 | Exception across boundary | CWE-248 | Any encoder/parser throw is caught at the binding |
| JSON-074 | Stack imbalance on error | CWE-664 | Decode error path leaves a balanced Lua stack |
| JSON-075 | Registry/temp leak | CWE-401 | Repeated encode/decode does not leak |
| JSON-076 | Reentrant encode via metatable | CWE-674 | Side effects of `__index` during traversal |
| JSON-077 | Concurrent encode (separate states) | CWE-362 | No shared mutable global |
| JSON-078 | OOM mid-encode | CWE-400 | Allocation failure handled cleanly |
| JSON-079 | OOM mid-decode | CWE-400 | Parser allocation failure handled |
| JSON-080 | Total-size DoS | CWE-400 | Huge document exhausts memory/CPU |
| JSON-081 | Integer overflow in size math | CWE-190 | Length arithmetic on huge inputs |
| JSON-082 | Output buffer reserve overflow | CWE-190 | The `reserve` size computation |
| JSON-083 | Round-trip stability | CWE-704 | The composition `decode∘encode` preserves structure |
| JSON-084 | Round-trip number fidelity | CWE-681 | Numeric values survive a round-trip |
| JSON-085 | Round-trip binary fidelity | CWE-626 | NUL/high bytes survive |
| JSON-086 | Huge key count map | CWE-407 | Hash behavior on many keys |
| JSON-087 | Pathological whitespace | CWE-400 | Megabytes of whitespace between tokens |
| JSON-088 | Alternating nest types | CWE-674 | Deep mixed nesting like `[{[{…}]}]` |
| JSON-089 | Repeated decode of same buffer | CWE-664 | Parser state not retained across calls |
| JSON-090 | Encode of decode output | CWE-704 | The output of `encode(decode(x))` stable |
| JSON-091 | Very deep but valid (≤ limit) | CWE-674 | Exactly-at-limit nesting accepted |
| JSON-092 | One-past-limit nesting | CWE-674 | Exactly-over-limit rejected |
| JSON-093 | Scientific notation extremes | CWE-681 | Values `1e-308`, `1e+308` |
| JSON-094 | Long fractional part | CWE-400 | Thousands of fraction digits |
| JSON-095 | Key ordering determinism | CWE-20 | Output ordering is stable/defined |
| JSON-096 | Unicode escape vs raw | CWE-176 | The escape `\u00e9` vs UTF-8 bytes equivalence |
| JSON-097 | Surrogate-pair round-trip | CWE-176 | Astral character fidelity |
| JSON-098 | Error message info leak | CWE-209 | Parse error text reveals no internals |
| JSON-099 | Dummy-driver build parity | CWE-697 | Non-nlohmann build fails safe (clear error) |
| JSON-100 | Fuzz corpus (structured + dumb) | CWE-20 | Random/grammar-based inputs never crash |

---

## Additional cases (documented quirks & interop hazards)

### Parser divergence ("Parsing JSON is a Minefield", Seriot)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-101 | Duplicate-key RFC ambiguity | CWE-436 | RFC 8259 leaves duplicates undefined → cross-impl disagreement |
| JSON-102 | Key order significance | CWE-436 | Consumers assume member order |
| JSON-103 | Number without fraction `1.` | CWE-20 | Trailing-dot number |
| JSON-104 | Leading-dot number `.5` | CWE-20 | Missing integer part |
| JSON-105 | Leading-zero `01` | CWE-20 | Octal-looking number |
| JSON-106 | Plus-prefixed number `+1` | CWE-20 | Explicit plus sign |
| JSON-107 | Hex/octal number literal | CWE-20 | A literal `0x1F` accepted by lax parsers |
| JSON-108 | Huge exponent `1e99999` | CWE-400 | Exponent blowup |
| JSON-109 | Negative zero vs zero | CWE-704 | The `-0` semantics |
| JSON-110 | Trailing comma | CWE-20 | The input `[1,2,]` accepted by lax parsers |
| JSON-111 | Leading comma | CWE-20 | The input `[,1]` |
| JSON-112 | Bare comment | CWE-20 | JSONC/JSON5 `//` and `/* */` leniency |
| JSON-113 | Single-quoted string | CWE-20 | Single quotes accepted by lax parsers |
| JSON-114 | Unquoted object key | CWE-20 | The input `{a:1}` |
| JSON-115 | Unescaped control in string | CWE-20 | Raw control byte inside quotes |
| JSON-116 | Unescaped tab/newline in string | CWE-20 | Literal control chars |
| JSON-117 | Bad `\u` escape | CWE-20 | The non-hex escape `\uXYZW` |
| JSON-118 | Lone high surrogate | CWE-176 | The escape `\uD800` without a low pair |
| JSON-119 | Lone low surrogate | CWE-176 | The escape `\uDC00` alone |
| JSON-120 | Reversed surrogate pair | CWE-176 | Low then high |
| JSON-121 | Overlong UTF-8 in input | CWE-176 | Non-shortest encoding |
| JSON-122 | UTF-16/UTF-32 BOM input | CWE-176 | Wrong-encoding document |
| JSON-123 | Truncated UTF-8 multibyte | CWE-176 | Cut mid-sequence |
| JSON-124 | Noncharacter `U+FFFE`/`U+FFFF` | CWE-176 | Noncharacters in strings |
| JSON-125 | Deeply nested (10k levels) | CWE-674 | Stack-depth divergence |
| JSON-126 | Huge array of zeros | CWE-400 | Parse-time/memory cost |
| JSON-127 | Whitespace flood | CWE-400 | Megabytes of inter-token space |
| JSON-128 | Mixed structural close | CWE-20 | Mismatched brackets in `{"a":1]` |
| JSON-129 | Multiple top-level values | CWE-20 | The input `1 2` / NDJSON confusion |
| JSON-130 | Empty document | CWE-20 | Empty input handling |
| JSON-131 | Only whitespace | CWE-20 | Whitespace-only input |
| JSON-132 | UTF-8 BOM accepted/rejected | CWE-20 | Leading BOM policy |
| JSON-133 | Number with many digits | CWE-400 | A 100k-digit integer |
| JSON-134 | Number with trailing junk | CWE-20 | The input `1abc` |
| JSON-135 | `Infinity`/`NaN` literal input | CWE-20 | Nonstandard tokens |
| JSON-136 | Escaped slash handling | CWE-116 | Optional `\/` escape |
| JSON-137 | NUL via `\u0000` escape | CWE-626 | Embedded NUL in a string |
| JSON-138 | Very deep but balanced | CWE-674 | Exactly at the nesting limit |

### Numbers & precision (IEEE-754 / interop)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-139 | Integer beyond 2^53 precision | CWE-681 | JS-interop precision loss |
| JSON-140 | Int64 id round-trip via float | CWE-681 | Large id corrupted |
| JSON-141 | Subnormal float | CWE-681 | Denormal handling |
| JSON-142 | Smallest/largest double | CWE-681 | Values `1e-308`/`1.7e308` |
| JSON-143 | Negative exponent extremes | CWE-681 | Underflow to zero |
| JSON-144 | Locale decimal separator | CWE-697 | Comma parsed/emitted by locale |
| JSON-145 | Float formatting round-trip | CWE-682 | Shortest-representation stability |
| JSON-146 | BigInt expectation | CWE-704 | An id treated as bignum |
| JSON-147 | Integer vs float type drift | CWE-704 | The value `3` becomes `3.0` |

### Logic / injection / interop

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-148 | JSON injection (concat build) | CWE-74 | Building JSON by string concatenation |
| JSON-149 | JSONP callback injection | CWE-79 | Reflected callback name |
| JSON-150 | Cross-site top-level array leak | CWE-200 | Array readable cross-origin (legacy) |
| JSON-151 | Mass assignment via JSON body | CWE-915 | Extra fields bound to a model |
| JSON-152 | Type juggling on decode | CWE-704 | String vs number confusion downstream |
| JSON-153 | Prototype-pollution analog | CWE-1321 | Abuse of a `__proto__`/metatable key in consumers |
| JSON-154 | Duplicate-key auth bypass | CWE-436 | Last-wins flips an auth flag |
| JSON-155 | Unicode-key collision bypass | CWE-178 | Normalized-equal keys |
| JSON-156 | Canonicalization (JCS) mismatch | RFC 8785 | Signed JSON re-serialized differently |
| JSON-157 | Signature-over-JSON malleability | CWE-347 | Re-encode changes bytes, breaks/forges a sig |
| JSON-158 | Number-as-key coercion | CWE-704 | Numeric-string keys |
| JSON-159 | Empty-key handling | CWE-20 | The object `{"":1}` |
| JSON-160 | Very long key | CWE-400 | Multi-MB key |
| JSON-161 | Many duplicate keys (hash flood) | CWE-407 | Object key collisions |
| JSON-162 | NDJSON/streaming confusion | CWE-436 | Concatenated objects |
| JSON-163 | Comment-based smuggling | CWE-436 | One parser keeps a comment another drops |

### Encode-side & round-trip

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-164 | HTML-unsafe chars unescaped | CWE-79 | Angle brackets/ampersand emitted into HTML |
| JSON-165 | `U+2028`/`U+2029` in output | CWE-79 | Line/paragraph separators break JS embedding |
| JSON-166 | Closing-tag in a string | CWE-79 | A `script` close sequence ends the tag |
| JSON-167 | Non-ASCII not escaped | CWE-176 | Downstream charset mismatch |
| JSON-168 | NaN/Inf emission policy | CWE-248 | Non-finite numbers |
| JSON-169 | Sparse-array emission | CWE-704 | Gaps change array vs object |
| JSON-170 | Metatable side effects on encode | CWE-913 | The `__index`/`__pairs` metamethods invoked |
| JSON-171 | Encode of recursive table | CWE-674 | Cycle handling |
| JSON-172 | Encode key-order nondeterminism | CWE-20 | Unstable output breaks signing |
| JSON-173 | Pretty-print option abuse | CWE-20 | Indent value extremes |
| JSON-174 | Round-trip NUL fidelity | CWE-626 | NUL survives encode then decode |
| JSON-175 | Round-trip large-number fidelity | CWE-681 | An id survives a round-trip |
| JSON-176 | Round-trip unicode fidelity | CWE-176 | Astral characters survive |
| JSON-177 | Differential encode/decode | CWE-697 | Encode after decode is not the identity |

### Memory / sanitizer / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| JSON-178 | nlohmann throw uncaught | CWE-248 | A type/parse error crosses the C boundary |
| JSON-179 | `dump()` invalid-UTF8 throw | CWE-248 | Error-handler not set |
| JSON-180 | Recursive-descent stack overflow | CWE-674 | Unbounded nesting (real CVE class) |
| JSON-181 | Pre-scan vs parser depth mismatch | CWE-697 | Guard depth differs from the library |
| JSON-182 | Allocation overflow on big number | CWE-190 | Digit-buffer sizing |
| JSON-183 | OOM on huge document | CWE-400 | Multi-GB input |
| JSON-184 | ASan trip in parser | CWE-125 | OOB read on malformed input |
| JSON-185 | UBSan trip on number cast | CWE-190 | Double-to-int UB |
| JSON-186 | Coverage-guided fuzz crash | CWE-20 | A libFuzzer corpus finding |
| JSON-187 | Differential vs reference parser | CWE-697 | Disagreement with jq/serde |
| JSON-188 | Stack imbalance on decode error | CWE-664 | Lua stack off after a throw |
| JSON-189 | Registry leak under churn | CWE-401 | Repeated decode leaks refs |
| JSON-190 | Reentrant encode via metamethod | CWE-674 | A metamethod `__index` re-enters encode |
| JSON-191 | Concurrent encode/decode | CWE-362 | Shared global state |
| JSON-192 | Huge key-count map perf | CWE-407 | Object-build complexity |
| JSON-193 | No-regex confirmation | CWE-1333 | Confirm the path has no backtracking regex |
| JSON-194 | Integer key vs string key | CWE-704 | Numeric vs string Lua keys on decode |
| JSON-195 | Empty array vs object decode | CWE-704 | The two empty shapes stay distinct |
| JSON-196 | Whitespace-driven parse cost | CWE-400 | Spacing between tokens |
| JSON-197 | Trailing data after value | CWE-20 | Strict end-of-input check |
| JSON-198 | Max-depth boundary | CWE-674 | At-limit vs over-limit behavior |
| JSON-199 | Dummy-driver build behavior | CWE-697 | Non-nlohmann build fails safe |
| JSON-200 | JSONTestSuite corpus | CWE-20 | Run the canonical `y_`/`n_`/`i_` test files |
