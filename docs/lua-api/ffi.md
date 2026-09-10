# 🧩 ffi

Declare C functions and call them directly from Lua.

```lua
local ffi = require("ffi")
ffi.cdef [[ unsigned long strlen(const char *s); ]]
print(ffi.C.strlen("hello")) -- 5
```

- `ffi.cdef(declarations)` — Register C declarations: functions, typedefs, structs and unions whose fields may be arrays, bitfields or pointers to functions, and enums.
- `ffi.C.<name>(...)` — Call a function from the default process namespace, which answers a symbol the host added with `Runtime::addSymbol` before it asks the dynamic linker. The same namespace answers the constants of every declared enum.
- `ffi.load(name)` — Load a shared library and return a namespace for its declared symbols (resolve the platform name with `platform.libraryFilename`). The path is UTF-8 on every platform, and a library that does not load is refused with its name and the reason the system gave.
- `ffi.new(ct [, n] [, init...])` — Allocate a cdata object of C type `ct`, such as `"sqlite3*[1]"` or `"unsigned char[?]"`.
- `ffi.cast(ct, value)` — Reinterpret a number, a pointer, a cdata or a light userdata as C type `ct`. When `ct` is a pointer to a function and `value` is a Lua function, the answer is a callback that keeps the function alive for as long as the cdata lives.
- `ffi.string(ptr [, len])` — Copy a C string, or `len` raw bytes, into a Lua string.
- `ffi.copy(dst, src [, len])` — Copy bytes into a cdata buffer.
- `ffi.offsetof(ct, field)` — Byte offset of a struct field. A bitfield answers three values, the offset of the unit that holds it, the bit it starts at inside that unit and its width.
- `ffi.sizeof`, `ffi.typeof`, `ffi.metatype`, `ffi.gc`, `ffi.istype`, `ffi.tonumber`, `ffi.fill`, `ffi.addressof`, `ffi.errno` — The rest of the introspection and memory surface.
- `ffi.nullptr` — The null pointer value.
- `ffi.VERSION` — The FFI backend version string.

This calls arbitrary native code by design. Treat declarations and inputs as trusted. In the browser build the module loads but its calls return a clear error.

## Types and calls

- A cdata whose type is a pointer to a function is called like a function, whether it came from `ffi.cast` or from a struct field, and a null one refuses the call with an error that says the pointer is null.
- A `T *` and a `T[n]` go where a `const T *` is expected, while a `const T *` is refused where a `T *` is expected unless `ffi.cast` drops the const. A `void *` takes and gives any pointer, as it does in C.
- An array field of a struct or a union reads and writes its elements in place and passes by value, since libffi sees it as its elements.
- An enum is sized like an `int`, or like an `unsigned int` when a constant is above the largest `int`, and its constants take decimal, octal or hexadecimal values with an optional minus sign.
- A bitfield of an integer type is laid out the way the C compiler of the platform lays it out, which on Windows opens a new unit whenever the declared type changes size. A bitfield wider than its type, one of no width, one of a type that is not an integer and one inside a packed record are refused.
- The types `intptr_t` and `uintptr_t` are integers as wide as a pointer.

## Callbacks

A Lua function made into a callback always runs on the main thread of the Lua state, never on the coroutine that made it, so it can be called after that coroutine ended. A callback that raises answers zero to its native caller, and its failure goes where someone can receive it:

- While an ffi call made from Lua runs, the failure is raised from that call once it returns, with the traceback of the callback after its message.
- Outside any ffi call, for example when a native library keeps the pointer and calls it later on the thread that runs Lua, the failure goes at once to the handler set with `async.onFailure(handler)`, which receives the error, the traceback and the frames, or else to the runtime, which logs it and stops the loop.
- From another thread the callback never touches the Lua state. It answers zero, and a message that names the callback type and says it was called from another thread reaches the same handler on the thread that runs Lua, with no traceback and no frames.

## Examples

### Buffer

```lua
-- Allocates a C buffer, copies bytes in, casts it, and reads it back via ffi.string.
local ffi = require("ffi")

local buf = ffi.new("char[?]", 6)
ffi.copy(buf, "world")

local ptr = ffi.cast("const char *", buf)
print("ffi buffer length:", ffi.sizeof("char[6]"))
print("ffi buffer text:", ffi.string(ptr))
print("ffi buffer prefix:", ffi.string(buf, 3))
```

### Convert

```lua
-- Fills a buffer, converts a cdata number to Lua, takes a pointer address, reads errno, registers a gc finalizer, and prints the backend version.
local ffi = require("ffi")

ffi.cdef [[
    int abs(int n);
]]

local buf = ffi.new("unsigned char[?]", 4)
ffi.fill(buf, 4, 0x41)
print("ffi convert fill:", ffi.string(buf, 4))

local n = ffi.new("int", 65)
print("ffi convert tonumber:", ffi.tonumber(n))
print("ffi convert addressof:", ffi.addressof(buf) ~= ffi.nullptr)

ffi.C.abs(-1)
print("ffi convert errno is number:", type(ffi.errno()) == "number")

local owned = ffi.new("char[?]", 1)
ffi.gc(owned, function() end)

print("ffi convert version:", ffi.VERSION)
print("ffi convert ok")
```

### Puts

```lua
-- Calls libc puts through the native ffi stack.
local ffi = require("ffi")

ffi.cdef [[
    int puts(const char *s);
]]

ffi.C.puts("ffi: hello from libc puts")
```

### SQLite

```lua
-- Runs SQLite create and select through native ffi bindings with the library name resolved by platform.libraryFilename.

local ffi = require("ffi")
local platform = require("platform")

local SQLITE_OK = 0
local SQLITE_ROW = 100
local SQLITE_DONE = 101
local SQLITE_INTEGER = 1
local SQLITE_FLOAT = 2
local SQLITE_TEXT = 3
local SQLITE_BLOB = 4
local SQLITE_NULL = 5
local SQLITE_OPEN_READWRITE = 0x00000002
local SQLITE_OPEN_CREATE = 0x00000004

ffi.cdef [[
typedef void sqlite3;
typedef void sqlite3_stmt;

int sqlite3_open_v2(
    const char *filename,
    sqlite3 **ppDb,
    int flags,
    const char *zVfs);

int sqlite3_close(sqlite3 *db);

int sqlite3_prepare_v2(
    sqlite3 *db,
    const char *zSql,
    int nByte,
    sqlite3_stmt **ppStmt,
    const char **pzTail);

int sqlite3_step(sqlite3_stmt *pStmt);
int sqlite3_finalize(sqlite3_stmt *pStmt);

int sqlite3_bind_int(sqlite3_stmt *pStmt, int i, int v);
int sqlite3_bind_double(sqlite3_stmt *pStmt, int i, double v);
int sqlite3_bind_text(sqlite3_stmt *pStmt, int i, const char *z, int n, void *xDel);
int sqlite3_bind_blob(sqlite3_stmt *pStmt, int i, const void *z, int n, void *xDel);
int sqlite3_bind_null(sqlite3_stmt *pStmt, int i);

int sqlite3_column_count(sqlite3_stmt *pStmt);
int sqlite3_column_type(sqlite3_stmt *pStmt, int iCol);
long long sqlite3_column_int64(sqlite3_stmt *pStmt, int iCol);
double sqlite3_column_double(sqlite3_stmt *pStmt, int iCol);
const unsigned char *sqlite3_column_text(sqlite3_stmt *pStmt, int iCol);
const void *sqlite3_column_blob(sqlite3_stmt *pStmt, int iCol);
int sqlite3_column_bytes(sqlite3_stmt *pStmt, int iCol);
]]

local S = ffi.load(platform.libraryFilename("sqlite3"))

local function must(rc, ctx)
  if rc ~= SQLITE_OK then
    error(ctx .. ": sqlite rc=" .. tostring(rc))
  end
end

local function type_name(code)
  if code == SQLITE_INTEGER then
    return "INTEGER"
  elseif code == SQLITE_FLOAT then
    return "REAL"
  elseif code == SQLITE_TEXT then
    return "TEXT"
  elseif code == SQLITE_BLOB then
    return "BLOB"
  elseif code == SQLITE_NULL then
    return "NULL"
  end
  return "TYPE(" .. tostring(code) .. ")"
end

local function format_cell(stmt, col)
  local t = S.sqlite3_column_type(stmt, col)
  if t == SQLITE_NULL then
    return "nil"
  elseif t == SQLITE_INTEGER then
    return tostring(S.sqlite3_column_int64(stmt, col))
  elseif t == SQLITE_FLOAT then
    return string.format("%g", S.sqlite3_column_double(stmt, col))
  elseif t == SQLITE_TEXT then
    local p = S.sqlite3_column_text(stmt, col)
    local n = S.sqlite3_column_bytes(stmt, col)
    if p == nil or n == 0 then
      return ""
    end
    return ffi.string(ffi.cast("const char *", p), n)
  elseif t == SQLITE_BLOB then
    local p = S.sqlite3_column_blob(stmt, col)
    local n = S.sqlite3_column_bytes(stmt, col)
    if p == nil or n == 0 then
      return "<empty blob>"
    end
    local hex = {}
    for i = 0, n - 1 do
      hex[#hex + 1] = string.format("%02x", ffi.cast("unsigned char*", p)[i])
    end
    return "0x" .. table.concat(hex)
  end
  return "?"
end

local db = ffi.new("sqlite3*[1]")
local flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE -- Lua 5.3+ bitwise
must(S.sqlite3_open_v2(":memory:", db, flags, nil), "open")
db = db[0]

local function run_sql(sql)
  local stmt = ffi.new("sqlite3_stmt*[1]")
  must(S.sqlite3_prepare_v2(db, sql, -1, stmt, nil), "prepare: " .. sql)
  stmt = stmt[0]
  local rc = S.sqlite3_step(stmt)
  if rc ~= SQLITE_DONE then
    S.sqlite3_finalize(stmt)
    error("step expected DONE for DDL, got " .. tostring(rc))
  end
  must(S.sqlite3_finalize(stmt), "finalize DDL")
end

run_sql [[
CREATE TABLE samples (
  id INTEGER NOT NULL,
  score REAL NOT NULL,
  label TEXT NOT NULL,
  payload BLOB NOT NULL,
  note INTEGER
);
]]

local ins = ffi.new("sqlite3_stmt*[1]")
must(
  S.sqlite3_prepare_v2(
    db,
    "INSERT INTO samples (id, score, label, payload, note) VALUES (?,?,?,?,?);",
    -1,
    ins,
    nil
  ),
  "prepare insert"
)
ins = ins[0]

must(S.sqlite3_bind_int(ins, 1, 7), "bind int")
must(S.sqlite3_bind_double(ins, 2, 2.718281828), "bind real")
must(S.sqlite3_bind_text(ins, 3, "hello ffi", -1, nil), "bind text") -- NULL destructor = SQLITE_STATIC
local blob = "\x00\xff\x01lua"
local blen = #blob
local buf = ffi.new("unsigned char[?]", blen)
ffi.copy(buf, blob, blen)
must(S.sqlite3_bind_blob(ins, 4, buf, blen, nil), "bind blob")
must(S.sqlite3_bind_null(ins, 5), "bind null")

local rc_ins = S.sqlite3_step(ins)
if rc_ins ~= SQLITE_DONE then
  S.sqlite3_finalize(ins)
  error("insert step: " .. tostring(rc_ins))
end
must(S.sqlite3_finalize(ins), "finalize insert")

local sel = ffi.new("sqlite3_stmt*[1]")
must(S.sqlite3_prepare_v2(db, "SELECT id, score, label, payload, note FROM samples;", -1, sel, nil), "prepare select")
sel = sel[0]

print("--- row(s) from SELECT (column index, declared type, sqlite3_column_type, value) ---")
while true do
  local rc = S.sqlite3_step(sel)
  if rc == SQLITE_DONE then
    break
  end
  if rc ~= SQLITE_ROW then
    S.sqlite3_finalize(sel)
    error("select step: " .. tostring(rc))
  end
  local n = S.sqlite3_column_count(sel)
  for c = 0, n - 1 do
    local t = S.sqlite3_column_type(sel, c)
    print(string.format("  col %d  %s  ->  %s", c, type_name(t), format_cell(sel, c)))
  end
end
must(S.sqlite3_finalize(sel), "finalize select")

must(S.sqlite3_close(db), "close")
print("ffi sqlite3: ok")
```

### Struct

```lua
-- Declares a C struct, attaches methods with metatype, and inspects its layout with typeof, sizeof, offsetof, and istype.
local ffi = require("ffi")

ffi.cdef [[
    typedef struct { int x; int y; } point_t;
]]

local point_t = ffi.typeof("point_t")
ffi.metatype(point_t, {
    __index = {
        sum = function(self)
            return self.x + self.y
        end,
    },
})

local p = ffi.new(point_t)
p.x = 3
p.y = 4

print("ffi struct sizeof:", ffi.sizeof("point_t"))
print("ffi struct offsetof y:", ffi.offsetof("point_t", "y"))
print("ffi struct istype:", ffi.istype(point_t, p))
print("ffi struct sum:", p:sum())
print("ffi struct ok")
```
### Function pointer

```lua
-- Calls a libc function through a pointer to it, and through a struct field that holds one.
local ffi = require("ffi")

ffi.cdef [[
    int abs(int n);
    typedef struct { int (*apply)(int); int value; } operation_t;
]]

local abs_pointer = ffi.cast("int (*)(int)", ffi.C.abs)
print("ffi pointer call:", abs_pointer(-5))

local operation = ffi.new("operation_t")
operation.apply = abs_pointer
operation.value = -8
print("ffi field call:", operation.apply(operation.value))

print("ffi null call:", pcall(ffi.new("int (*)(int)"), 1))
```

### Light userdata

```cpp
// The engine hands Lua the address of a native function and of a counter as light userdata through a module of its own.
static int counter = 41;

static int negate(int value)
{
    return -value;
}

static int openNative(lua_State* L)
{
    lua_newtable(L);
    lua_pushlightuserdata(L, &counter);
    lua_setfield(L, -2, "counter");
    lua_pushlightuserdata(L, reinterpret_cast<void*>(&negate));
    lua_setfield(L, -2, "negate");
    return 1;
}

varn::runtime::Runtime runtime({"game"});

if (!runtime.addModule("engine.native", &openNative))
{
    // The name is one a module of Varn or an earlier call already answers.
}

runtime.runScript("main.lua");
```

```lua
-- Casts light userdata to a data pointer and to a function pointer that both point at the address the engine gave.
local ffi = require("ffi")
local native = require("engine.native")

local counter = ffi.cast("int *", native.counter)
counter[0] = counter[0] + 1
print("ffi light userdata counter:", counter[0])

local negate = ffi.cast("int (*)(int)", native.negate)
print("ffi light userdata call:", negate(5))
```

### Const pointer

```lua
-- Passes an array and a pointer where a pointer to const is expected, and shows a pointer to const refused where a mutable one is expected.
local ffi = require("ffi")

ffi.cdef [[
    unsigned long strlen(const char *s);
    typedef void (*clear_fn)(int *values, int count);
]]

local text = ffi.new("char[16]", "hello")
print("ffi const array:", ffi.C.strlen(text))
print("ffi const pointer:", ffi.C.strlen(ffi.cast("char *", text)))

local values = ffi.new("int[4]", { 1, 2, 3, 4 })
local clear = ffi.cast("clear_fn", function() end)
print("ffi const refused:", pcall(clear, ffi.cast("const int *", values), 4))
```

### Struct array

```lua
-- Lays out a struct whose fields are arrays, sorts an array of them and passes one by value through a function pointer.
local ffi = require("ffi")

ffi.cdef [[
    typedef struct { int values[4]; char name[16]; } record_t;
    typedef record_t (*scale_fn)(record_t record, int factor);
    void qsort(void *base, unsigned long count, unsigned long size, int (*compare)(const void *, const void *));
]]

print("ffi struct array sizeof:", ffi.sizeof("record_t"))
print("ffi struct array offsetof name:", ffi.offsetof("record_t", "name"))

local records = ffi.new("record_t[3]")
for index, value in ipairs({ 30, 10, 20 }) do
    records[index - 1].values[0] = value
    records[index - 1].name = "record " .. value
end

local compare = ffi.cast("int (*)(const void *, const void *)", function(a, b)
    return ffi.cast("const record_t *", a).values[0] - ffi.cast("const record_t *", b).values[0]
end)
ffi.C.qsort(records, 3, ffi.sizeof("record_t"), compare)
print("ffi struct array first:", ffi.string(records[0].name))

local scale = ffi.cast("scale_fn", function(input, factor)
    local output = ffi.new("record_t", input)
    for index = 0, 3 do
        output.values[index] = input.values[index] * factor
    end
    return output
end)
print("ffi struct array by value:", scale(records[0], 3).values[0])
```

### Callback failure

```lua
-- Raises the failure of a comparator from the call to qsort with the traceback of the comparator, and sets the handler that a failure outside any call reaches.
local ffi = require("ffi")
local async = require("async")

ffi.cdef [[
    void qsort(void *base, unsigned long count, unsigned long size, int (*compare)(const void *, const void *));
]]

async.onFailure(function(err, traceback)
    print("ffi callback failed outside a call:", err)
end)

local values = ffi.new("int[3]", { 3, 1, 2 })
local failing = ffi.cast("int (*)(const void *, const void *)", function()
    error("The comparator gave up.")
end)

local ok, err = pcall(ffi.C.qsort, values, 3, ffi.sizeof("int"), failing)
print("ffi callback failed inside a call:", ok, err)
```

### Enum

```lua
-- Declares an enum, reads its constants through ffi.C and passes it through a function pointer.
local ffi = require("ffi")

ffi.cdef [[
    enum color { RED, GREEN = 4, BLUE };
    typedef enum color (*next_color_fn)(enum color current);
]]

print("ffi enum constants:", ffi.C.RED, ffi.C.GREEN, ffi.C.BLUE)
print("ffi enum sizeof:", ffi.sizeof("enum color"))

local next_color = ffi.cast("next_color_fn", function(current)
    return current == ffi.C.RED and ffi.C.GREEN or ffi.C.BLUE
end)
print("ffi enum call:", next_color(ffi.C.RED))
```

### Bitfield

```lua
-- Declares a struct of bitfields, writes and reads them, and asks where one sits.
local ffi = require("ffi")

ffi.cdef [[
    typedef struct { unsigned flags : 3; unsigned mode : 5; int level : 4; } header_t;
]]

local header = ffi.new("header_t")
header.flags = 5
header.mode = 17
header.level = -3

print("ffi bitfield sizeof:", ffi.sizeof("header_t"))
print("ffi bitfield values:", header.flags, header.mode, header.level)
print("ffi bitfield offsetof mode:", ffi.offsetof("header_t", "mode"))
print("ffi bitfield too wide:", pcall(ffi.cdef, "struct too_wide { unsigned char flags : 9; };"))
```

### Pointer-wide integer

```lua
-- Converts an address to a pointer-wide integer and back.
local ffi = require("ffi")

local buffer = ffi.new("int[1]", { 42 })
local address = ffi.cast("uintptr_t", buffer)

print("ffi intptr sizeof:", ffi.sizeof("intptr_t"), ffi.sizeof("void *"))
print("ffi intptr round trip:", ffi.cast("int *", address)[0])
```

### Static symbol

```cpp
// The engine adds a function that no library exports, under a name of its choice.
extern "C" int engine_triple(int value)
{
    return value * 3;
}

varn::runtime::Runtime runtime({"game"});

if (!runtime.addSymbol("engine_triple", reinterpret_cast<void*>(&engine_triple)))
{
    // The name was already added with another address.
}

runtime.runScript("main.lua");
```

```lua
-- Calls the function the engine added through the default namespace.
local ffi = require("ffi")

ffi.cdef [[
    int engine_triple(int value);
]]

print("ffi static symbol:", ffi.C.engine_triple(14))
```

### Library path

```lua
-- Loads a library from a UTF-8 path and prints the reason the system gives when it does not load.
local ffi = require("ffi")

print("ffi load refused:", pcall(ffi.load, "/opt/bibliothèque/libmissing.so"))
```

## Under the hood

Built on libffi.
