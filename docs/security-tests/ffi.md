# 🧩 ffi

`ffi.cdef[[...]]`, `ffi.C.<name>(...)`, `ffi.nullptr`. Calls arbitrary native code by
design; tests target the declaration parser and the argument/return marshalling layer.

### Declaration parser (`cdef`)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-001 | Malformed declaration | CWE-20 | garbage cdef crashes the parser |
| FFI-002 | Unterminated declaration | CWE-20 | missing `;`/`)` |
| FFI-003 | Deeply nested types | CWE-674 | `int*****...` depth overflow |
| FFI-004 | Huge declaration | CWE-400 | multi-MB cdef exhausts memory |
| FFI-005 | Token-lexer overflow | CWE-121 | overlong identifier/literal |
| FFI-006 | Comment/preprocessor edge | CWE-20 | `//`, `/* */`, `#` directives |
| FFI-007 | Redefinition | CWE-20 | re-declaring a symbol with a new signature |
| FFI-008 | Conflicting prototype | CWE-843 | declaration mismatches the real ABI |
| FFI-009 | Variadic function decl | CWE-20 | `printf(const char*, ...)` handling |
| FFI-010 | Struct/union layout | CWE-188 | packing/alignment assumptions |
| FFI-011 | Enum/typedef resolution | CWE-20 | unresolved typedef |
| FFI-012 | Function pointer types | CWE-843 | callback signature parsing |
| FFI-013 | Bitfield handling | CWE-188 | bitfield layout |
| FFI-014 | Array-in-struct sizing | CWE-190 | array size overflow in layout |
| FFI-015 | NUL in cdef text | CWE-626 | embedded NUL truncates parsing |
| FFI-016 | Unicode in identifiers | CWE-20 | non-ASCII identifier handling |
| FFI-017 | Reentrant cdef | CWE-674 | cdef invoked during cdef |
| FFI-018 | Parser memory leak | CWE-401 | repeated cdef leaks |
| FFI-019 | Parser fuzz | CWE-20 | random C-like text never crashes |
| FFI-020 | Integer literal overflow | CWE-190 | huge array/enum constant |

### Argument marshalling

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-021 | Null pointer arg | CWE-476 | passing `nil`/nullptr where deref'd |
| FFI-022 | Dangling pointer arg | CWE-416 | a freed buffer passed to C |
| FFI-023 | Type confusion (string↔ptr) | CWE-843 | string passed where a struct ptr expected |
| FFI-024 | Wrong arity (too few) | CWE-20 | missing arguments read garbage |
| FFI-025 | Wrong arity (too many) | CWE-20 | extra arguments |
| FFI-026 | Integer truncation 64→32 | CWE-190 | large Lua integer into a 32-bit arg |
| FFI-027 | Sign mismatch | CWE-194 | negative into unsigned arg |
| FFI-028 | Float↔int coercion | CWE-681 | number coerced to the wrong C type |
| FFI-029 | Pointer arithmetic from Lua | CWE-468 | offset pointer escapes the object |
| FFI-030 | Buffer too small (out-param) | CWE-119 | C writes past a Lua-provided buffer |
| FFI-031 | String not NUL-terminated | CWE-170 | C reads past a non-terminated string |
| FFI-032 | Embedded NUL in string arg | CWE-626 | truncation at NUL for C string args |
| FFI-033 | Lua string lifetime | CWE-416 | C holds a pointer past GC of the string |
| FFI-034 | Large array arg | CWE-789 | huge array marshalling |
| FFI-035 | Struct-by-value | CWE-20 | by-value struct ABI handling |
| FFI-036 | Vararg type promotion | CWE-686 | varargs promotion rules |
| FFI-037 | Bool/char width | CWE-20 | 1-byte type marshalling |
| FFI-038 | Endianness in marshalling | CWE-188 | byte order of multi-byte args |
| FFI-039 | Alignment of args | CWE-188 | misaligned pointer args |
| FFI-040 | Const-correctness ignored | CWE-20 | writing through a const pointer |

### Return values & callbacks

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-041 | Return pointer lifetime | CWE-416 | returned pointer used after free |
| FFI-042 | Return string ownership | CWE-401 | who frees a returned `char*` |
| FFI-043 | Return truncation | CWE-197 | wide return truncated to Lua number |
| FFI-044 | Return type confusion | CWE-843 | pointer returned as number |
| FFI-045 | errno/last-error handling | CWE-703 | error code lost after the call |
| FFI-046 | Callback into Lua | CWE-662 | C calls back into Lua mid-operation |
| FFI-047 | Callback lifetime | CWE-416 | Lua callback used after its state is gone |
| FFI-048 | Callback exception | CWE-248 | a Lua error thrown inside a C callback |
| FFI-049 | Reentrancy via callback | CWE-674 | callback re-enters the FFI |
| FFI-050 | Callback stack overflow | CWE-674 | deep C↔Lua recursion |
| FFI-051 | Thread context of callback | CWE-362 | callback fires on the wrong thread |
| FFI-052 | Long-lived callback leak | CWE-401 | registered callbacks accumulate |

### Library loading

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-053 | Untrusted library name | CWE-829 | loading an attacker-named library |
| FFI-054 | Relative library path | CWE-426 | search-path hijack |
| FFI-055 | `LD_LIBRARY_PATH` influence | CWE-426 | env alters which lib loads |
| FFI-056 | DLL search order (Windows) | CWE-427 | planted DLL in the search path |
| FFI-057 | Symbol resolution to wrong lib | CWE-427 | symbol clobbered by another module |
| FFI-058 | Load nonexistent symbol | CWE-20 | missing symbol handled, no crash |
| FFI-059 | Load failure handling | CWE-703 | `dlopen` failure surfaced |
| FFI-060 | Version/ABI mismatch | CWE-843 | loaded lib has a different ABI |
| FFI-061 | Unload/`dlclose` safety | CWE-416 | calling into an unloaded lib |
| FFI-062 | Path with control chars | CWE-74 | control bytes in a library path |

### Memory safety in the binding

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-063 | Marshalling buffer overflow | CWE-120 | arg-staging buffer overrun |
| FFI-064 | Marshalling OOB read | CWE-125 | reading past an arg array |
| FFI-065 | Use-after-free of arg buffer | CWE-416 | arg buffer freed before the call |
| FFI-066 | Double free of arg buffer | CWE-415 | cleanup runs twice |
| FFI-067 | Uninitialized arg slot | CWE-457 | unset arg passed |
| FFI-068 | Integer overflow in size calc | CWE-190 | `count*size` for an array arg |
| FFI-069 | Type-tag confusion | CWE-843 | mismatched internal type tag |
| FFI-070 | Stack imbalance on error | CWE-664 | a failed call leaves the Lua stack off |
| FFI-071 | Exception across boundary | CWE-248 | C++/Lua error crosses the FFI |
| FFI-072 | Leak on call error | CWE-401 | staging memory leaked when a call fails |

### Concurrency & misc

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-073 | Concurrent calls | CWE-362 | thread-unsafe C function called concurrently |
| FFI-074 | Shared global state | CWE-362 | C lib global races |
| FFI-075 | Signal handling in C | CWE-364 | C installs a handler that breaks the VM |
| FFI-076 | Blocking C call on main loop | CWE-400 | a long C call stalls the event loop |
| FFI-077 | Recursion depth via C | CWE-674 | C recursion overflows the native stack |
| FFI-078 | `ffi.nullptr` misuse | CWE-476 | arithmetic/deref on the null sentinel |
| FFI-079 | Pointer-to-Lua-string write | CWE-787 | C writes into an immutable Lua string |
| FFI-080 | GC during a call | CWE-416 | GC collects an arg mid-call |
| FFI-081 | Finalizer ordering | CWE-416 | `__gc` frees a buffer still in use |
| FFI-082 | Huge return array | CWE-400 | returning a giant array |
| FFI-083 | Format-string via FFI | CWE-134 | calling `printf`-family with user data |
| FFI-084 | Arbitrary memory read | CWE-125 | `memcpy`-style read from a chosen address |
| FFI-085 | Arbitrary memory write | CWE-787 | `memcpy`-style write to a chosen address |
| FFI-086 | Arbitrary `mmap`/`mprotect` | CWE-269 | making memory executable |
| FFI-087 | `system`/`exec` reachable | CWE-78 | command execution by design |
| FFI-088 | File ops via FFI | CWE-73 | bypassing the fs module's checks |
| FFI-089 | Network via FFI | CWE-918 | raw sockets bypass socket-module checks |
| FFI-090 | Privilege change via FFI | CWE-269 | `setuid`-style calls |
| FFI-091 | Sandbox escape | CWE-265 | FFI defeats any Lua sandbox |
| FFI-092 | Symbol address disclosure | CWE-200 | leaking pointer values (ASLR) |
| FFI-093 | Struct field OOB | CWE-125 | reading a field past the struct |
| FFI-094 | Negative array index | CWE-129 | negative index into a C array |
| FFI-095 | Off-by-one in array bounds | CWE-193 | `arr[len]` access |
| FFI-096 | NaN/Inf to integer arg | CWE-681 | non-finite number to an int parameter |
| FFI-097 | Wide-char/UTF-16 args | CWE-176 | `wchar_t*` marshalling |
| FFI-098 | Calling convention mismatch | CWE-843 | cdecl vs stdcall (Windows) |
| FFI-099 | Error-message info leak | CWE-209 | binding errors reveal addresses/internals |
| FFI-100 | Fuzz cdef + call | CWE-20 | random declarations and arguments never crash the binding |

---

## Additional cases (deeper / documented)

### Sandbox / RCE surface (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-101 | FFI reachable in a sandbox | CWE-265 | FFI defeats any Lua sandbox (Redis Lua-class) |
| FFI-102 | `system`/`exec` symbol | CWE-78 | declaring and calling `system` |
| FFI-103 | `mprotect` to RWX | CWE-269 | making attacker bytes executable |
| FFI-104 | `mmap`/`memfd` shellcode | CWE-94 | map + jump to code |
| FFI-105 | `dlopen` arbitrary lib | CWE-829 | load any shared object |
| FFI-106 | `dlsym` arbitrary symbol | CWE-829 | resolve and call any symbol |
| FFI-107 | GOT/PLT overwrite | CWE-471 | write a function pointer table |
| FFI-108 | vtable overwrite | CWE-787 | corrupt a C++ vtable |
| FFI-109 | Return-oriented chaining | CWE-1321 | pivot via controlled pointers |
| FFI-110 | Arbitrary read primitive | CWE-125 | `memcpy` from a chosen address |
| FFI-111 | Arbitrary write primitive | CWE-787 | `memcpy` to a chosen address |
| FFI-112 | Leak ASLR base | CWE-200 | print a symbol/pointer address |
| FFI-113 | Disable mitigations | CWE-693 | `prctl`/seccomp tampering |
| FFI-114 | setuid/cap change | CWE-269 | privilege escalation syscalls |
| FFI-115 | ptrace/process inject | CWE-749 | attach to another process |

### Declaration parser (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-116 | Macro/preprocessor abuse | CWE-20 | `#define` tricks in cdef |
| FFI-117 | Typedef recursion | CWE-674 | mutually recursive typedefs |
| FFI-118 | Anonymous struct/union | CWE-20 | anonymous member layout |
| FFI-119 | Flexible array member | CWE-130 | trailing `[]` sizing |
| FFI-120 | `#pragma pack` layout | CWE-188 | packing changes offsets |
| FFI-121 | Bitfield ordering | CWE-188 | endianness of bitfields |
| FFI-122 | Enum underlying type | CWE-194 | enum width assumptions |
| FFI-123 | Function-pointer typedef | CWE-843 | callback type parsing |
| FFI-124 | Variadic prototype | CWE-686 | `...` argument promotion |
| FFI-125 | `const`/`volatile` qualifiers | CWE-20 | qualifier handling |
| FFI-126 | Nested pointer depth | CWE-674 | `int*****` parser depth |
| FFI-127 | Array-size overflow | CWE-190 | huge array dimension |
| FFI-128 | Incomplete type use | CWE-20 | using a forward-declared type |
| FFI-129 | Duplicate symbol redefine | CWE-694 | re-cdef changes a signature |
| FFI-130 | cdef parser fuzz | CWE-20 | random C text never crashes |

### Marshalling / ABI (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-131 | cdecl vs stdcall (Win32) | CWE-843 | calling-convention mismatch |
| FFI-132 | System V vs Win64 ABI | CWE-843 | register/stack arg mismatch |
| FFI-133 | Struct-by-value ABI | CWE-188 | small-struct passing rules |
| FFI-134 | Return-struct hidden pointer | CWE-188 | sret convention |
| FFI-135 | Float/SSE register class | CWE-188 | float args in the wrong class |
| FFI-136 | Vararg float promotion | CWE-686 | float→double in varargs |
| FFI-137 | Long-double size variance | CWE-188 | 80/128-bit long double |
| FFI-138 | `size_t`/`long` width | CWE-188 | LP64 vs LLP64 |
| FFI-139 | Alignment requirements | CWE-1257 | misaligned arg pointers |
| FFI-140 | Endianness of multi-byte args | CWE-188 | byte order mismatch |
| FFI-141 | Sign extension of small ints | CWE-194 | char/short promotion |
| FFI-142 | Pointer truncation 64→32 | CWE-197 | pointer stored in a 32-bit slot |
| FFI-143 | Null-terminated string contract | CWE-170 | non-terminated `char*` |
| FFI-144 | Wide-char marshalling | CWE-176 | `wchar_t*` width/encoding |
| FFI-145 | Out-param buffer size | CWE-119 | C writes past a small buffer |
| FFI-146 | In/out buffer aliasing | CWE-758 | overlapping in/out pointers |
| FFI-147 | Array length mismatch | CWE-130 | declared vs passed length |
| FFI-148 | NUL in string arg | CWE-626 | truncation for C strings |
| FFI-149 | Lua string immutability write | CWE-787 | C writes into an interned string |
| FFI-150 | GC of arg during call | CWE-416 | buffer collected mid-call |

### Lifetime / callbacks / memory (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-151 | Return pointer lifetime | CWE-416 | use a returned pointer after free |
| FFI-152 | Caller-vs-callee free | CWE-401 | ownership of a returned buffer |
| FFI-153 | Callback after state gone | CWE-416 | Lua callback outlives its state |
| FFI-154 | Callback throws into C | CWE-248 | a Lua error inside a C callback |
| FFI-155 | Reentrant callback | CWE-674 | callback re-enters FFI |
| FFI-156 | Deep C↔Lua recursion | CWE-674 | callback recursion overflows |
| FFI-157 | Callback wrong thread | CWE-362 | fired off the main loop |
| FFI-158 | Long-lived callback leak | CWE-401 | callbacks accumulate |
| FFI-159 | Staging-buffer overflow | CWE-120 | arg-staging overrun |
| FFI-160 | Staging-buffer UAF | CWE-416 | freed before the call |
| FFI-161 | Staging double-free | CWE-415 | cleanup runs twice |
| FFI-162 | Uninitialized arg slot | CWE-457 | unset argument |
| FFI-163 | Arg-array OOB | CWE-125 | reading past the arg array |
| FFI-164 | Integer overflow in arg sizing | CWE-190 | `count*size` for arrays |
| FFI-165 | Type-tag confusion | CWE-843 | mismatched internal tag |
| FFI-166 | `ffi.nullptr` arithmetic | CWE-476 | offset/deref of null |
| FFI-167 | Negative array index | CWE-129 | `arr[-1]` |
| FFI-168 | Off-by-one array bound | CWE-193 | `arr[len]` |
| FFI-169 | Struct-field OOB | CWE-125 | field past struct end |
| FFI-170 | Double-call on one buffer | CWE-664 | reused staging across calls |

### Library loading / supply (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-171 | DLL search-order hijack | CWE-427 | planted DLL (Windows) |
| FFI-172 | LD_PRELOAD/LD_LIBRARY_PATH | CWE-426 | env-driven lib hijack |
| FFI-173 | RPATH/RUNPATH abuse | CWE-426 | binary RPATH points to a writable dir |
| FFI-174 | Relative library path | CWE-426 | cwd-relative lib load |
| FFI-175 | dlopen of a writable lib | CWE-427 | attacker-writable .so |
| FFI-176 | Symbol clobber across libs | CWE-471 | duplicate symbol resolved wrong |
| FFI-177 | ABI/version mismatch | CWE-843 | wrong lib version loaded |
| FFI-178 | dlclose then call | CWE-416 | call into an unloaded lib |
| FFI-179 | Unicode/control in lib path | CWE-74 | control bytes in the path |
| FFI-180 | Constructor/init side effects | CWE-829 | `.so` ctor runs on load |

### Concurrency / errors / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-181 | Thread-unsafe C func concurrent | CWE-362 | shared C global races |
| FFI-182 | errno not thread-local handling | CWE-362 | errno clobbered across threads |
| FFI-183 | Blocking C call on main loop | CWE-400 | long call stalls the loop |
| FFI-184 | Signal handler from C | CWE-364 | C installs a handler |
| FFI-185 | longjmp from C over Lua | CWE-758 | non-local jump corrupts state |
| FFI-186 | Exception across boundary | CWE-248 | C++ throw via FFI |
| FFI-187 | Stack imbalance on call error | CWE-664 | failed call leaves Lua stack off |
| FFI-188 | Leak on call error | CWE-401 | staging leaked on failure |
| FFI-189 | Format-string via FFI printf | CWE-134 | user data to a printf-family call |
| FFI-190 | ASan trip in marshalling | CWE-125 | OOB in arg staging |
| FFI-191 | UBSan trip on cast | CWE-190 | numeric conversion UB |
| FFI-192 | TSan race in callbacks | CWE-362 | callback data race |
| FFI-193 | Differential ABI test | CWE-697 | result mismatch vs a reference call |
| FFI-194 | Error-message address leak | CWE-209 | error reveals pointers (ASLR) |
| FFI-195 | NaN/Inf to int arg | CWE-681 | non-finite to integer parameter |
| FFI-196 | Huge return array | CWE-400 | giant returned array |
| FFI-197 | Recursion via destructor | CWE-674 | `__gc` frees in-use memory |
| FFI-198 | Finalizer ordering | CWE-416 | `__gc` frees a buffer still used |
| FFI-199 | Dummy-driver behavior | CWE-697 | non-FFI build fails safe |
| FFI-200 | Fuzz cdef + call + lib | CWE-20 | random declarations/args/libs never crash the binding |

---

## Round 3 — deeper / documented

### RCE / sandbox-escape surface (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-201 | `execve`/`posix_spawn` | CWE-78 | process creation |
| FFI-202 | `fork` + `exec` chain | CWE-78 | spawn a child |
| FFI-203 | `dlopen` of a writable `.so` | CWE-427 | load attacker code |
| FFI-204 | `LoadLibrary` planted DLL | CWE-427 | Windows search-order hijack |
| FFI-205 | `mprotect` page to RWX | CWE-1303 | mark memory executable |
| FFI-206 | `mmap` PROT_EXEC region | CWE-94 | map shellcode |
| FFI-207 | GOT/PLT entry overwrite | CWE-471 | hijack a resolved call |
| FFI-208 | C++ vtable overwrite | CWE-787 | corrupt a virtual dispatch |
| FFI-209 | `setjmp`/`longjmp` pivot | CWE-758 | control-flow pivot |
| FFI-210 | `syscall()` direct | CWE-749 | bypass libc wrappers |
| FFI-211 | `ptrace` self/other | CWE-749 | process inspection/injection |
| FFI-212 | `prctl`/seccomp tamper | CWE-693 | weaken sandbox |
| FFI-213 | Leak symbol address (ASLR) | CWE-200 | defeat randomization |
| FFI-214 | Arbitrary read via `memcpy` | CWE-125 | read a chosen address |
| FFI-215 | Arbitrary write via `memcpy` | CWE-787 | write a chosen address |
| FFI-216 | File ops bypass fs checks | CWE-73 | `open`/`unlink` directly |
| FFI-217 | Raw socket bypass | CWE-918 | `connect`/`sendto` directly |
| FFI-218 | `setuid`/`setgid` | CWE-269 | privilege change |

### Declaration parser (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-219 | Recursive typedef | CWE-674 | mutually recursive types |
| FFI-220 | Deeply nested pointers | CWE-674 | `int**********` |
| FFI-221 | Huge array dimension | CWE-190 | overflow in layout |
| FFI-222 | Anonymous struct/union | CWE-20 | anonymous member layout |
| FFI-223 | Flexible array member | CWE-130 | trailing `[]` sizing |
| FFI-224 | `#pragma pack` offsets | CWE-188 | packing changes layout |
| FFI-225 | Bitfield ordering/endianness | CWE-188 | bitfield layout |
| FFI-226 | Enum width assumption | CWE-194 | underlying type |
| FFI-227 | Variadic prototype | CWE-686 | `...` promotion |
| FFI-228 | Incomplete-type use | CWE-20 | forward-declared type |
| FFI-229 | Macro/preprocessor abuse | CWE-20 | `#define` tricks |
| FFI-230 | NUL in cdef text | CWE-626 | truncated parsing |
| FFI-231 | Overlong identifier | CWE-121 | lexer buffer overrun |
| FFI-232 | Reentrant cdef | CWE-674 | cdef during cdef |
| FFI-233 | cdef parser fuzz | CWE-20 | random declarations |

### Marshalling / ABI (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-234 | cdecl vs stdcall (Win32) | CWE-843 | convention mismatch |
| FFI-235 | SysV vs Win64 ABI | CWE-843 | register/stack mismatch |
| FFI-236 | Struct-by-value passing | CWE-188 | small-struct rules |
| FFI-237 | sret hidden pointer | CWE-188 | return-struct convention |
| FFI-238 | Float/SSE register class | CWE-188 | wrong register class |
| FFI-239 | Long-double size variance | CWE-188 | 80/128-bit |
| FFI-240 | LP64 vs LLP64 `long` | CWE-188 | width mismatch |
| FFI-241 | Alignment of pointer args | CWE-1257 | misaligned access |
| FFI-242 | Sign-extension of small ints | CWE-194 | char/short promotion |
| FFI-243 | Pointer truncation 64→32 | CWE-197 | pointer in a 32-bit slot |
| FFI-244 | NUL-terminated string contract | CWE-170 | non-terminated `char*` |
| FFI-245 | NUL in string arg | CWE-626 | truncation |
| FFI-246 | Wide-char marshalling | CWE-176 | `wchar_t*` width |
| FFI-247 | Out-param buffer size | CWE-119 | C writes past the buffer |
| FFI-248 | In/out buffer aliasing | CWE-758 | overlapping pointers |
| FFI-249 | Array length mismatch | CWE-130 | declared vs passed length |
| FFI-250 | Lua string immutability write | CWE-787 | write into an interned string |
| FFI-251 | GC of arg during call | CWE-416 | buffer collected mid-call |
| FFI-252 | NaN/Inf to int arg | CWE-681 | non-finite to integer |
| FFI-253 | Integer overflow in arg sizing | CWE-190 | `count*size` |

### Lifetime / callbacks / memory (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-254 | Return-pointer lifetime | CWE-416 | use after free |
| FFI-255 | Returned-buffer ownership | CWE-401 | who frees it |
| FFI-256 | Return truncation | CWE-197 | wide return to a Lua number |
| FFI-257 | Return type confusion | CWE-843 | pointer as number |
| FFI-258 | errno not preserved | CWE-703 | last-error lost |
| FFI-259 | Callback after state gone | CWE-416 | callback outlives the state |
| FFI-260 | Callback throws into C | CWE-248 | Lua error in a C callback |
| FFI-261 | Reentrant callback | CWE-674 | callback re-enters FFI |
| FFI-262 | Deep C↔Lua recursion | CWE-674 | stack overflow |
| FFI-263 | Callback wrong thread | CWE-362 | off-main-loop callback |
| FFI-264 | Long-lived callback leak | CWE-401 | callbacks accumulate |
| FFI-265 | Staging-buffer overflow | CWE-120 | arg-staging overrun |
| FFI-266 | Staging-buffer UAF | CWE-416 | freed before the call |
| FFI-267 | Staging double-free | CWE-415 | cleanup runs twice |
| FFI-268 | Uninitialized arg slot | CWE-457 | unset argument |
| FFI-269 | Type-tag confusion | CWE-843 | internal tag mismatch |
| FFI-270 | Negative array index | CWE-129 | `arr[-1]` |
| FFI-271 | Off-by-one array bound | CWE-193 | `arr[len]` |
| FFI-272 | Struct-field OOB | CWE-125 | field past struct end |
| FFI-273 | Finalizer ordering | CWE-416 | `__gc` frees in-use memory |

### Library loading / concurrency / fuzz (deeper)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| FFI-274 | RPATH/RUNPATH abuse | CWE-426 | writable RPATH dir |
| FFI-275 | LD_PRELOAD hijack | CWE-426 | env preloads code |
| FFI-276 | Relative library path | CWE-426 | cwd-relative load |
| FFI-277 | Symbol clobber across libs | CWE-471 | duplicate symbol wrong |
| FFI-278 | ABI/version mismatch | CWE-843 | wrong lib version |
| FFI-279 | dlclose then call | CWE-416 | call into an unloaded lib |
| FFI-280 | Constructor side effects on load | CWE-829 | `.so` ctor runs |
| FFI-281 | Control bytes in lib path | CWE-74 | injection in the path |
| FFI-282 | Thread-unsafe C func concurrent | CWE-362 | shared C global races |
| FFI-283 | errno thread-locality | CWE-362 | errno across threads |
| FFI-284 | Blocking C call on main loop | CWE-400 | long call stalls the loop |
| FFI-285 | Signal handler installed by C | CWE-364 | handler breaks the VM |
| FFI-286 | longjmp over Lua frames | CWE-758 | non-local jump corrupts state |
| FFI-287 | Exception across boundary | CWE-248 | C++ throw via FFI |
| FFI-288 | Stack imbalance on call error | CWE-664 | failed call leaves stack off |
| FFI-289 | Leak on call error | CWE-401 | staging leaked |
| FFI-290 | Format-string via printf | CWE-134 | user data to printf-family |
| FFI-291 | ASan trip in marshalling | CWE-125 | OOB in staging |
| FFI-292 | UBSan trip on cast | CWE-190 | numeric conversion UB |
| FFI-293 | TSan race in callbacks | CWE-362 | callback data race |
| FFI-294 | Differential ABI test | CWE-697 | mismatch vs a reference call |
| FFI-295 | Error reveals address | CWE-209 | pointer in an error (ASLR) |
| FFI-296 | Huge return array | CWE-400 | giant returned array |
| FFI-297 | Recursion via destructor | CWE-674 | `__gc` recursion |
| FFI-298 | `ffi.nullptr` arithmetic | CWE-476 | offset/deref of null |
| FFI-299 | Dummy-driver behavior | CWE-697 | non-FFI build fails safe |
| FFI-300 | Fuzz cdef + call + lib | CWE-20 | random inputs never crash the binding |
