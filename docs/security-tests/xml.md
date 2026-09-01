# 🧬 xml

`xml.encode(node [,{pretty,indent}])` / `xml.decode(text)` and aliases. Node model
`{name, attributes, children, text}` over pugixml.

### XXE & DTD / entities

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-001 | XXE local file read | CWE-611 | `<!ENTITY x SYSTEM "file:///etc/passwd">` exposes file |
| XML-002 | XXE SSRF | CWE-611 | external entity URL fetches an internal resource |
| XML-003 | XXE via parameter entity | CWE-611 | `% pe` parameter entity exfiltration |
| XML-004 | Blind XXE (OOB) | CWE-611 | out-of-band exfil through an external DTD |
| XML-005 | External DTD load | CWE-611 | `<!DOCTYPE x SYSTEM "http://evil/x.dtd">` |
| XML-006 | Public DTD / catalog | CWE-611 | `PUBLIC` identifier triggers a fetch |
| XML-007 | Billion laughs | CWE-776 | nested entity expansion exhausts memory |
| XML-008 | Quadratic blowup | CWE-776 | one huge entity referenced many times |
| XML-009 | Recursive entity | CWE-674 | self/mutually referential entities |
| XML-010 | Internal subset processing | CWE-611 | `[ ... ]` internal subset honored |
| XML-011 | Entity in attribute | CWE-611 | entity reference inside an attribute value |
| XML-012 | Predefined-entity confusion | CWE-20 | `&lt;`/`&#x...;` mishandling |
| XML-013 | Numeric char ref overflow | CWE-190 | `&#x110000;` beyond Unicode max |
| XML-014 | UTF-16/BOM-declared XXE | CWE-611 | alternate encoding hides the DOCTYPE |

### Parser robustness & injection

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-015 | XInclude | CWE-611 | `xi:include` pulls in a file |
| XML-016 | XSLT processing | CWE-611 | stylesheet PI executes/reads |
| XML-017 | Processing-instruction abuse | CWE-20 | `<?php ?>`/PI mishandling |
| XML-018 | CDATA edge | CWE-20 | nested/unterminated `<![CDATA[` |
| XML-019 | Comment edge | CWE-20 | `--` inside / unterminated comment |
| XML-020 | Namespace confusion | CWE-20 | duplicate/redefined prefixes |
| XML-021 | Duplicate attributes | CWE-20 | `<a x="1" x="2">` |
| XML-022 | Attribute value quotes | CWE-20 | unquoted / mixed-quote attributes |
| XML-023 | Mismatched tags | CWE-20 | `<a></b>` rejected |
| XML-024 | Unclosed tag | CWE-20 | `<a>` rejected |
| XML-025 | Multiple roots | CWE-20 | two top-level elements |
| XML-026 | No root element | CWE-20 | text-only / empty document rejected |
| XML-027 | Garbage prolog | CWE-20 | junk before `<?xml` |
| XML-028 | Invalid char in name | CWE-20 | control/space in a tag name |
| XML-029 | Overlong element name | CWE-400 | multi-MB tag name |
| XML-030 | Huge attribute count | CWE-400 | thousands of attributes on one element |
| XML-031 | Huge attribute value | CWE-400 | multi-MB attribute value |
| XML-032 | Deeply nested elements | CWE-674 | `<a>`×N node conversion depth-capped |
| XML-033 | Wide sibling list | CWE-400 | millions of sibling elements |
| XML-034 | Mixed content ordering | CWE-20 | text interleaved with elements |
| XML-035 | Whitespace-only text nodes | CWE-20 | significant vs insignificant whitespace |
| XML-036 | Encoding declaration mismatch | CWE-176 | declared encoding != bytes |
| XML-037 | Invalid UTF-8 input | CWE-176 | malformed bytes rejected/handled |
| XML-038 | NUL byte in document | CWE-626 | embedded NUL handling |
| XML-039 | BOM handling | CWE-20 | leading BOM accepted/rejected |
| XML-040 | Empty input | CWE-20 | `""` rejected |
| XML-041 | Non-XML input | CWE-20 | `not xml` rejected, no crash |
| XML-042 | Truncated document | CWE-20 | cut-off mid-tag |
| XML-043 | Bare `&` / unescaped specials | CWE-20 | invalid raw `&`/`<` in text |
| XML-044 | Astral/surrogate chars | CWE-176 | 4-byte UTF-8 in text/attrs |
| XML-045 | Entity-count limit | CWE-776 | many distinct entity definitions |

### Encode (node model → XML)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-046 | Output injection in text | CWE-91 | `<`/`>`/`&` in text must be escaped |
| XML-047 | Output injection in attribute | CWE-91 | quotes/`<`/`&` in attribute values escaped |
| XML-048 | Element-name injection | CWE-91 | invalid chars in `name` sanitized |
| XML-049 | Attribute-name injection | CWE-91 | invalid chars in attribute keys sanitized |
| XML-050 | Empty / missing name | CWE-20 | node without `name` defaults safely |
| XML-051 | Numeric-leading name | CWE-20 | name starting with a digit fixed |
| XML-052 | Reserved `xml` prefix | CWE-20 | `xml`-prefixed names handled |
| XML-053 | CDATA round-trip | CWE-20 | text with `]]>` does not break output |
| XML-054 | Comment-like text | CWE-20 | `--`/`<!--` in text escaped, not a comment |
| XML-055 | PI-like text | CWE-20 | `<?...?>` in text escaped |
| XML-056 | NUL in text/attr | CWE-626 | NUL handling (XML can't carry it) |
| XML-057 | Non-string text value | CWE-20 | number/boolean text coercion |
| XML-058 | Non-table node | CWE-20 | encode rejects a non-table input |
| XML-059 | children not an array | CWE-20 | malformed `children` handled |
| XML-060 | attributes not a table | CWE-20 | malformed `attributes` handled |
| XML-061 | Deep node encode | CWE-674 | encode depth-capped, no overflow |
| XML-062 | Cyclic node graph | CWE-674 | self-referential node does not infinite-loop |
| XML-063 | Wide node encode | CWE-400 | millions of children bounded |
| XML-064 | Encode mutates input | CWE-664 | node table not modified during encode |
| XML-065 | Pretty indent huge/negative | CWE-20 | indent option bounds |
| XML-066 | Invalid UTF-8 in node values | CWE-176 | raw bytes in text/attrs handled |

### Type conversion & round-trip

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-067 | name fidelity | CWE-704 | tag name preserved decode→encode |
| XML-068 | attribute fidelity | CWE-704 | attributes preserved and unescaped |
| XML-069 | child order fidelity | CWE-704 | document order preserved in `children` |
| XML-070 | text concatenation | CWE-704 | split text nodes concatenated correctly |
| XML-071 | duplicate-name siblings | CWE-704 | repeated child names kept as array entries |
| XML-072 | escaped-char round-trip | CWE-116 | `<`,`>`,`&`,`"` survive a round-trip |
| XML-073 | whitespace fidelity | CWE-20 | meaningful whitespace preserved |
| XML-074 | empty element | CWE-20 | `<a/>` vs `<a></a>` |
| XML-075 | attribute-only element | CWE-20 | element with attrs, no children/text |
| XML-076 | nested round-trip | CWE-704 | multi-level structure stable |

### Memory / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-077 | Exception across boundary | CWE-248 | pugixml/throw caught at the binding |
| XML-078 | Stack imbalance on error | CWE-664 | decode error leaves a balanced Lua stack |
| XML-079 | `lua_checkstack` on deep parse | CWE-674 | reservation before recursive push |
| XML-080 | Registry/temp leak | CWE-401 | repeated encode/decode no leak |
| XML-081 | OOM mid-parse | CWE-400 | allocation failure handled |
| XML-082 | OOM mid-encode | CWE-400 | allocation failure handled |
| XML-083 | Reentrant encode via metatable | CWE-674 | `__index` side effects during traversal |
| XML-084 | Concurrent parse (separate states) | CWE-362 | no shared mutable global |
| XML-085 | Integer overflow in size math | CWE-190 | length math on huge documents |
| XML-086 | At-limit nesting accepted | CWE-674 | exactly-at-cap depth ok |
| XML-087 | Over-limit nesting bounded | CWE-674 | exactly-over-cap stops safely |
| XML-088 | Pathological whitespace | CWE-400 | megabytes of inter-tag whitespace |
| XML-089 | Many tiny elements | CWE-407 | parse-time complexity bound |
| XML-090 | Long text node | CWE-400 | multi-GB text content |
| XML-091 | Attribute hash flood | CWE-407 | colliding attribute names |
| XML-092 | Namespace explosion | CWE-400 | thousands of namespace decls |
| XML-093 | Error message info leak | CWE-209 | parse error reveals no internals |
| XML-094 | Dummy-driver build parity | CWE-697 | non-pugixml build fails safe |
| XML-095 | Mixed encode/decode stability | CWE-704 | `decode(encode(x))` stable |
| XML-096 | Repeated parse of same buffer | CWE-664 | no retained state |
| XML-097 | Control bytes in input | CWE-74 | raw control chars rejected/escaped |
| XML-098 | Overlong UTF-8 in names | CWE-176 | overlong encodings rejected |
| XML-099 | DOCTYPE without entities | CWE-20 | bare DOCTYPE handled safely |
| XML-100 | Fuzz corpus (grammar + dumb) | CWE-20 | random/grammar inputs never crash |

---

## Additional cases (documented attacks & CVEs)

### XXE / DTD (documented variants)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-101 | Classic XXE file read | CWE-611 | external general entity reads a local file |
| XML-102 | Out-of-band XXE | CWE-611 | external DTD exfiltrates via a callback URL |
| XML-103 | Parameter-entity XXE | CWE-611 | `%`-entities in the internal subset |
| XML-104 | Error-based XXE | CWE-611 | parse error leaks file content |
| XML-105 | XXE via xinclude | CWE-611 | `xi:include href="file://"` |
| XML-106 | XXE via XSD schemaLocation | CWE-611 | schema fetch from an attacker host |
| XML-107 | XXE via XSLT document() | CWE-611 | stylesheet reads a file |
| XML-108 | XXE in SVG/Office/DOCX | CWE-611 | XML-bearing formats |
| XML-109 | XXE in SOAP body | CWE-611 | web-service XML |
| XML-110 | UTF-16 DOCTYPE smuggling | CWE-611 | encoding hides the DOCTYPE from filters |
| XML-111 | UTF-7 DOCTYPE smuggling | CWE-611 | charset trick reveals a DOCTYPE |
| XML-112 | Nested external DTD chain | CWE-611 | DTD pulls another DTD |
| XML-113 | php://filter-style wrapper | CWE-611 | wrapper-based file read (consumer ext) |
| XML-114 | jar:/netdoc: protocol | CWE-611 | JVM-style protocol handlers |
| XML-115 | expect:// via XXE | CWE-78 | command exec via a PHP expect wrapper |

### Entity expansion / DoS (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-116 | Billion laughs (exponential) | CWE-776 | nested entity expansion |
| XML-117 | Quadratic blowup | CWE-776 | one large entity referenced many times |
| XML-118 | External-entity amplification | CWE-776 | fetched entity reused |
| XML-119 | Recursive parameter entity | CWE-674 | self-referential `%pe` |
| XML-120 | Entity-count limit | CWE-776 | many entity definitions |
| XML-121 | Deeply nested elements | CWE-674 | nesting overflows the recursive consumer |
| XML-122 | Wide sibling explosion | CWE-400 | millions of siblings |
| XML-123 | Huge attribute count | CWE-400 | thousands of attributes |
| XML-124 | Huge text node | CWE-400 | multi-GB content |
| XML-125 | Comment/CDATA bomb | CWE-400 | giant comment or CDATA |
| XML-126 | Namespace-prefix explosion | CWE-400 | thousands of namespace declarations |
| XML-127 | Attribute-value normalization cost | CWE-407 | expensive normalization |
| XML-128 | Hash flood on attribute names | CWE-407 | colliding attribute keys |

### Signature / canonicalization (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-129 | XML signature wrapping (XSW) | CWE-347 | move the signed element, inject a new one |
| XML-130 | SAML assertion wrapping | CWE-347 | duplicate-id signature bypass |
| XML-131 | C14N normalization mismatch | CWE-347 | canonicalization differs from signer |
| XML-132 | Comment-insertion in signed node | CWE-347 | comment splits a text node (Cisco/Duo class) |
| XML-133 | ID attribute confusion | CWE-347 | reference resolves to the wrong element |
| XML-134 | Transform algorithm abuse | CWE-347 | malicious signature transforms |
| XML-135 | KeyInfo trust confusion | CWE-347 | attacker-supplied verification key |
| XML-136 | Detached vs enveloped confusion | CWE-347 | signature scope ambiguity |

### Parser robustness / encoding (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-137 | Encoding mislabel (UTF-7) | CWE-176 | declared vs actual encoding |
| XML-138 | BOM vs declaration conflict | CWE-176 | BOM contradicts `encoding=` |
| XML-139 | Invalid char references | CWE-20 | `&#x0;`/out-of-range code points |
| XML-140 | Numeric ref beyond U+10FFFF | CWE-190 | overflow in char-ref parsing |
| XML-141 | Mixed-case/whitespace DOCTYPE | CWE-20 | obfuscated DOCTYPE |
| XML-142 | Duplicate attribute names | CWE-20 | `x="1" x="2"` |
| XML-143 | Namespace redefinition | CWE-20 | prefix rebound mid-document |
| XML-144 | Default-namespace confusion | CWE-20 | empty-prefix scoping |
| XML-145 | PI target `xml` reserved | CWE-20 | `<?xml ... ?>` mid-document |
| XML-146 | Processing-instruction abuse | CWE-20 | unexpected PI handling |
| XML-147 | Conditional sections (INCLUDE/IGNORE) | CWE-20 | DTD conditional sections |
| XML-148 | Notation/unparsed entity | CWE-20 | NDATA entity handling |
| XML-149 | General vs parameter entity scope | CWE-20 | wrong-context entity use |
| XML-150 | Whitespace significance | CWE-20 | xml:space handling |
| XML-151 | Mixed content ordering | CWE-20 | text interleaved with elements |
| XML-152 | Unterminated CDATA | CWE-20 | `<![CDATA[` without close |
| XML-153 | Nested CDATA close `]]>` | CWE-20 | premature CDATA end |
| XML-154 | Astral chars in names | CWE-176 | 4-byte UTF-8 element names |
| XML-155 | Control chars in content | CWE-74 | disallowed XML control chars |
| XML-156 | Truncated multibyte | CWE-176 | cut UTF-8 sequence |
| XML-157 | Trailing data after root | CWE-20 | content after the root element |
| XML-158 | Multiple roots | CWE-20 | two top-level elements |
| XML-159 | Garbage before prolog | CWE-20 | junk before `<?xml` |
| XML-160 | Extremely long token | CWE-400 | multi-MB name/value |

### Encode-side injection (node model)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-161 | Text-content injection | CWE-91 | unescaped `<`/`&` in text |
| XML-162 | Attribute-value injection | CWE-91 | unescaped quotes in attributes |
| XML-163 | Element-name injection | CWE-91 | invalid chars in a tag name |
| XML-164 | Attribute-name injection | CWE-91 | invalid chars in an attribute key |
| XML-165 | CDATA-close injection | CWE-91 | `]]>` in text |
| XML-166 | Comment injection | CWE-91 | `--`/`-->` in text |
| XML-167 | PI injection | CWE-91 | `<?`/`?>` in text |
| XML-168 | DOCTYPE injection on encode | CWE-91 | smuggling a DOCTYPE via a value |
| XML-169 | Namespace-prefix injection | CWE-91 | `:` in a name |
| XML-170 | Reserved-name `xml*` | CWE-20 | `xmlns`/`xml:` names |
| XML-171 | NUL in node value | CWE-626 | NUL handling (XML forbids it) |
| XML-172 | Non-string node value | CWE-20 | numeric/boolean coercion |
| XML-173 | Encode of recursive node | CWE-674 | cyclic node graph |
| XML-174 | Encode depth cap | CWE-674 | over-deep node tree |
| XML-175 | Pretty-print option abuse | CWE-20 | indent extremes |

### Memory / sanitizer / concurrency / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-176 | pugixml throw uncaught | CWE-248 | a library error crosses the boundary |
| XML-177 | Stack imbalance on parse error | CWE-664 | Lua stack off after an error |
| XML-178 | checkstack before deep push | CWE-674 | reservation before recursion |
| XML-179 | Registry leak under churn | CWE-401 | repeated parse leaks refs |
| XML-180 | OOM mid-parse | CWE-400 | allocation failure handled |
| XML-181 | OOM mid-encode | CWE-400 | allocation failure handled |
| XML-182 | ASan trip on malformed input | CWE-125 | OOB read in parsing |
| XML-183 | UBSan trip on char-ref math | CWE-190 | code-point arithmetic UB |
| XML-184 | Reentrant encode via metamethod | CWE-674 | `__index` re-enters encode |
| XML-185 | Concurrent parse (separate states) | CWE-362 | shared global state |
| XML-186 | Integer overflow in size math | CWE-190 | document-size arithmetic |
| XML-187 | At-limit nesting accepted | CWE-674 | exactly-at-cap depth |
| XML-188 | Over-limit nesting bounded | CWE-674 | exactly-over-cap |
| XML-189 | Coverage-guided fuzz crash | CWE-20 | libFuzzer corpus finding |
| XML-190 | Differential vs reference parser | CWE-697 | disagreement with libxml2 |
| XML-191 | Round-trip name fidelity | CWE-704 | tag name preserved |
| XML-192 | Round-trip attribute fidelity | CWE-704 | attributes preserved |
| XML-193 | Round-trip text/escape fidelity | CWE-116 | special chars survive |
| XML-194 | Child-order fidelity | CWE-704 | document order preserved |
| XML-195 | Duplicate-name siblings | CWE-704 | repeated child names kept |
| XML-196 | Error-message info leak | CWE-209 | error text reveals internals |
| XML-197 | Dummy-driver build behavior | CWE-697 | non-pugixml build fails safe |
| XML-198 | Mojibake/non-UTF8 names | CWE-176 | invalid-encoding names |
| XML-199 | xmlconf W3C test suite | CWE-20 | run the canonical conformance corpus |
| XML-200 | DTD/entity disabled assertion | CWE-611 | assert external entities never load |

---

## Round 3 — deeper / documented

### XXE / DTD (deeper variants)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-201 | XXE via SVG render path | CWE-611 | SVG with an external entity |
| XML-202 | XXE via DOCX/XLSX/ODF | CWE-611 | office-doc XML members |
| XML-203 | XXE via SAML response | CWE-611 | assertion XML |
| XML-204 | XXE via SOAP envelope | CWE-611 | web-service XML |
| XML-205 | XXE via RSS/Atom | CWE-611 | feed parsing |
| XML-206 | XXE via XML-RPC | CWE-611 | RPC payload |
| XML-207 | OOB XXE through parameter entity | CWE-611 | `%`-entity exfil |
| XML-208 | Error-based file disclosure | CWE-611 | parse error leaks content |
| XML-209 | Recursive external DTD | CWE-776 | DTD fetches DTD |
| XML-210 | UTF-16BE/LE DOCTYPE hide | CWE-611 | encoding hides the directive |
| XML-211 | UTF-7 DOCTYPE hide | CWE-611 | charset trick |
| XML-212 | jar:/netdoc:/expect: wrapper | CWE-611 | protocol-handler abuse (consumer) |
| XML-213 | data: URI entity | CWE-611 | inline-data external id |
| XML-214 | Conditional-section DTD | CWE-611 | INCLUDE/IGNORE sections |
| XML-215 | Notation/unparsed entity | CWE-20 | NDATA handling |
| XML-216 | Public-id catalog resolution | CWE-611 | XML catalog fetch |
| XML-217 | Local-DTD gadget (existing file) | CWE-611 | reuse a system DTD for exfil |
| XML-218 | XInclude with xpointer | CWE-611 | partial file inclusion |
| XML-219 | XInclude text vs xml mode | CWE-611 | raw-text inclusion |
| XML-220 | XSLT document()/unparsed-text() | CWE-611 | stylesheet file read |

### Entity expansion / DoS (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-221 | Exponential entity (billion laughs) | CWE-776 | nested expansion |
| XML-222 | Quadratic single-entity reuse | CWE-776 | one big entity many times |
| XML-223 | External-general-entity amplification | CWE-776 | fetched then reused |
| XML-224 | Parameter-entity recursion | CWE-674 | recursive `%pe` |
| XML-225 | Attribute-value expansion | CWE-776 | entity inside an attribute |
| XML-226 | Deeply nested elements | CWE-674 | overflow the consumer recursion |
| XML-227 | Wide-sibling explosion | CWE-400 | millions of siblings |
| XML-228 | Huge attribute count | CWE-400 | thousands per element |
| XML-229 | Huge single text node | CWE-400 | multi-GB content |
| XML-230 | Namespace-decl explosion | CWE-400 | thousands of xmlns |
| XML-231 | Comment/CDATA giant block | CWE-400 | giant comment |
| XML-232 | Entity-count limit | CWE-776 | many entity definitions |
| XML-233 | Attribute normalization cost | CWE-407 | expensive normalization |

### Signature / canonicalization (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-234 | XSW: sibling injection | CWE-347 | add a forged element beside the signed one |
| XML-235 | XSW: wrap signed element | CWE-347 | move the original under a new root |
| XML-236 | XSW: duplicate Id | CWE-347 | two elements share the reference Id |
| XML-237 | SAML assertion-wrapping | CWE-347 | authn-bypass via XSW |
| XML-238 | C14N exclusive vs inclusive | CWE-347 | canonicalization mismatch |
| XML-239 | Comment-splitting in signed text | CWE-347 | comment changes the read value |
| XML-240 | Transform chain abuse | CWE-347 | malicious signature transforms |
| XML-241 | KeyInfo attacker key | CWE-347 | verifier trusts embedded key |
| XML-242 | RetrievalMethod SSRF | CWE-918 | key fetched from a URL |
| XML-243 | Reference URI dereference | CWE-918 | external reference fetch |

### Parser robustness / encoding (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-244 | expat/libxml2 CVE surface | CWE-1395 | keep the parser patched |
| XML-245 | Billion-laughs default guard | CWE-776 | confirm expansion disabled |
| XML-246 | Encoding sniffing mismatch | CWE-176 | declared vs actual encoding |
| XML-247 | BOM contradicts declaration | CWE-176 | BOM vs `encoding=` |
| XML-248 | Numeric char-ref overflow | CWE-190 | `&#x110000;` |
| XML-249 | Invalid XML char in content | CWE-20 | disallowed control char |
| XML-250 | Surrogate in char-ref | CWE-176 | `&#xD800;` |
| XML-251 | Duplicate attribute | CWE-20 | `x="1" x="2"` |
| XML-252 | Namespace rebinding mid-doc | CWE-20 | prefix redefined |
| XML-253 | Default-namespace empty | CWE-20 | undeclare default ns |
| XML-254 | PI target reserved (`xml`) | CWE-20 | misplaced declaration |
| XML-255 | Mismatched/Unclosed tags | CWE-20 | structural error |
| XML-256 | Multiple roots | CWE-20 | two top-level elements |
| XML-257 | Garbage before/after prolog | CWE-20 | junk around the document |
| XML-258 | NUL byte in document | CWE-626 | embedded NUL |
| XML-259 | Truncated multibyte | CWE-176 | cut UTF-8 sequence |
| XML-260 | Overlong UTF-8 in names | CWE-176 | overlong encoding |
| XML-261 | Extremely long token | CWE-400 | multi-MB name/value |
| XML-262 | Whitespace flood | CWE-400 | megabytes of inter-tag space |
| XML-263 | Many tiny elements | CWE-407 | parse-time complexity |

### Encode-side injection (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-264 | Text `<`/`>`/`&` injection | CWE-91 | unescaped special chars |
| XML-265 | Attribute quote injection | CWE-91 | unescaped quotes |
| XML-266 | Element-name injection | CWE-91 | invalid chars in a name |
| XML-267 | Attribute-name injection | CWE-91 | invalid chars in a key |
| XML-268 | CDATA-close in text | CWE-91 | `]]>` in content |
| XML-269 | Comment sequence in text | CWE-91 | `-->` in content |
| XML-270 | PI sequence in text | CWE-91 | `?>` in content |
| XML-271 | DOCTYPE smuggling via value | CWE-91 | injecting a directive |
| XML-272 | Namespace-prefix injection | CWE-91 | `:` in a name |
| XML-273 | Reserved `xml`/`xmlns` name | CWE-20 | reserved-name handling |
| XML-274 | NUL in node value | CWE-626 | NUL handling |
| XML-275 | Non-string node value | CWE-20 | numeric/boolean coercion |
| XML-276 | Recursive node graph | CWE-674 | cyclic node |
| XML-277 | Encode depth cap | CWE-674 | over-deep node tree |
| XML-278 | Pretty-indent extremes | CWE-20 | indent option bounds |

### Memory / concurrency / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| XML-279 | pugixml throw uncaught | CWE-248 | a library error crosses the boundary |
| XML-280 | Stack imbalance on parse error | CWE-664 | Lua stack off after an error |
| XML-281 | checkstack before deep push | CWE-674 | reservation before recursion |
| XML-282 | Registry leak under churn | CWE-401 | repeated parse leaks |
| XML-283 | OOM mid-parse | CWE-400 | allocation failure |
| XML-284 | OOM mid-encode | CWE-400 | allocation failure |
| XML-285 | Reentrant encode via metamethod | CWE-674 | `__index` re-enters |
| XML-286 | Concurrent parse | CWE-362 | shared global state |
| XML-287 | Integer overflow in size math | CWE-190 | document-size arithmetic |
| XML-288 | At-limit nesting | CWE-674 | exactly at cap |
| XML-289 | Over-limit nesting | CWE-674 | exactly over cap |
| XML-290 | ASan trip on malformed input | CWE-125 | OOB read |
| XML-291 | UBSan trip on char-ref math | CWE-190 | code-point UB |
| XML-292 | Differential vs libxml2 | CWE-697 | parsing disagreement |
| XML-293 | Round-trip name fidelity | CWE-704 | tag name preserved |
| XML-294 | Round-trip attribute fidelity | CWE-704 | attributes preserved |
| XML-295 | Round-trip escape fidelity | CWE-116 | special chars survive |
| XML-296 | Child-order fidelity | CWE-704 | order preserved |
| XML-297 | Duplicate-name siblings | CWE-704 | kept as array entries |
| XML-298 | Error-message info leak | CWE-209 | no internals in errors |
| XML-299 | Dummy-driver build behavior | CWE-697 | non-pugixml build fails safe |
| XML-300 | Fuzz parse + encode | CWE-20 | grammar/dumb fuzz never crashes |
