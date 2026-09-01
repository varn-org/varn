# 🧩 ffi

Declare C functions and call them directly from Lua.

```lua
local ffi = require("ffi")
ffi.cdef [[ unsigned long strlen(const char *s); ]]
print(ffi.C.strlen("hello")) -- 5
```

- `ffi.cdef(declarations)` — register C declarations.
- `ffi.C.<name>(...)` — call a function from the default process namespace.
- `ffi.load(name)` — load a shared library and return a namespace for its declared symbols (resolve the platform name with `platform.libraryFilename`).
- `ffi.new(ct [, n] [, init...])` — allocate a cdata object of C type `ct`, such as `"sqlite3*[1]"` or `"unsigned char[?]"`.
- `ffi.cast(ct, value)` — reinterpret a value or pointer as C type `ct`.
- `ffi.string(ptr [, len])` — copy a C string, or `len` raw bytes, into a Lua string.
- `ffi.copy(dst, src [, len])` — copy bytes into a cdata buffer.
- `ffi.sizeof`, `ffi.typeof`, `ffi.offsetof`, `ffi.metatype`, `ffi.gc`, `ffi.istype`, `ffi.tonumber`, `ffi.fill`, `ffi.addressof`, `ffi.errno` — the rest of the introspection and memory surface.
- `ffi.nullptr` — the null pointer value.
- `ffi.VERSION` — the FFI backend version string.

This calls arbitrary native code by design; treat declarations and inputs as trusted. In the
browser build the module loads but its calls return a clear error.

## Examples

### `puts.lua`

```lua
-- exercises libc puts through the native ffi stack when that build links it
local ffi = require("ffi")

ffi.cdef [[
    int puts(const char *s);
]]

ffi.C.puts("ffi: hello from libc puts")

```

### `sqlite3.lua`

```lua
-- walks sqlite create and select with native ffi bindings, then exits explicitly.
-- the library name is resolved per platform through platform.libraryFilename.

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

## Under the hood

Built on libffi.
