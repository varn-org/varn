# 🪟 dataview

Memory that Lua and the host share by reference instead of copying it. A data view is an address, a size and an id that no other view of the process ever takes. Lua, C, C++, Objective-C, Swift, Java, Kotlin and JavaScript each hold a reference to the same bytes, so a large buffer such as a camera frame or a decoded image crosses between them without a copy and without turning into text. The module is available in every build, the browser included.

```lua
local dataview = require("dataview")

local frame = dataview.new(16)
frame:write(0, "RGBA")
print(frame.size, frame:read(0, 4))
```

## Functions

- `dataview.new(size)` → A view of `size` zeroed bytes. A negative size raises `A data view needs a size of zero or more bytes, not -1.`, and memory the system cannot give raises `A data view of N bytes could not be allocated.`.
- `dataview.from(text)` → A view that holds a copy of the bytes of a string.
- `dataview.is(value)` → Whether a value is a data view, released or not.

## A view

- `view.size` and `#view` → The size in bytes.
- `view.pointer` → The address of the first byte as a light userdata. The function `ffi.cast("uint8_t *", view.pointer)` turns it into a pointer that reads and writes the bytes in place. The pointer is valid only while the view lives, so keep the view in a variable for as long as the pointer is used.
- `view.id` → The id of the view.
- `view[offset]` and `view[offset] = byte` → Reads or writes the byte at `offset` in place, as an integer from `0` to `255`, without copying anything into a string. Offsets count bytes from zero, as memory does, and this is the way to change shared bytes from Lua where `ffi` is absent, such as in the browser. A value outside `0` to `255` raises `A byte of a data view holds an integer from 0 to 255, not 256.`, and assigning a field by name raises `A data view takes a byte at an offset, not the field "size".`.
- `view:read(offset?, length?)` → A string with a copy of `length` bytes at `offset`. Offsets count bytes from zero, as memory does. The offset defaults to `0` and the length to the rest of the view.
- `view:write(offset, text)` → Copies the bytes of `text` into the view at `offset`.
- `view:release()` → Drops the reference of this value now, instead of when the value is collected. Releasing again does nothing.
- `view.released` → Whether this value released its reference.
- `tostring(view)` → `dataview (N bytes)`, or `dataview (released)`.
- `view == other` → Whether both values hold the same view, such as one view that reached Lua twice.

A byte read or written at an offset outside the view raises `The offset 8 lies outside the data view of 8 bytes.`. A range that passes the end raises `The range of 2 bytes at offset 7 passes the end of the data view of 8 bytes.`, and an offset outside the view raises `The offset 9 lies outside the data view of 8 bytes.`. Every use of a released value but `release`, `released` and `tostring` raises `The data view was released.`.

```lua
local dataview = require("dataview")
local ffi = require("ffi")

local view = dataview.new(4)
local bytes = ffi.cast("uint8_t *", view.pointer)
bytes[0] = 72
bytes[1] = 105
print(view:read(0, 2)) -- Hi
view:release()
print(view.released) -- true
```

```lua
local dataview = require("dataview")

-- Indexing changes the shared bytes in place in every build, the browser included.
local view = dataview.from("\0\64\255")
for i = 0, #view - 1 do
    view[i] = 255 - view[i]
end
print(view[0], view[1], view[2]) -- 255 191 0
```

## Ownership and lifetime

- **Every holder owns one reference.** A Lua value, a C++ `varn::dataview::DataView`, a `varn_dataview*` of the C API, an Objective-C or Swift `VarnDataView`, a Java or Kotlin `VarnDataView` and a JavaScript `VarnDataView` each own one. References are counted atomically, so they are taken and dropped on any thread.
- **The last reference frees the memory.** Memory that `dataview.new` or `varn_dataview_new` allocated is freed, and memory that a host wrapped goes back to its owner through the function it gave, on the thread that dropped the last reference.
- **JSON carries a token, never a reference.** The token is `{"$view": 7, "pointer": "0x10a3f0000", "size": 4096}`, with the pointer as hexadecimal text, since an address of 64 bits does not survive the numbers of every language. Whoever sends a token keeps the view alive until the call that carries it returns, and the receiver takes its own reference from the token before that. A token names a view only while it lives and only with its exact address and size, and the id of a view is never reused, so a stale or forged token reaches no memory.
- **The bytes are not synchronized.** A view shares memory, so the writer finishes writing before it hands the view over, and readers and writers that work at the same time agree on their own order, such as a ring of views that a producer reuses only once every consumer released it.

Plain JSON never meets a view. The function `json.encode` refuses one, and `json.decode` reads a token as a plain table, so the address of shared memory never reaches the network and JSON from the network never reaches memory of the process. Only the bridge of the host writes and reads tokens.

## The bridge of the host

A view crosses the bridge in both directions, inside the JSON of a host function and of an event.

- **Lua to the host.** A view in the argument of `host.<name>` reaches the host as its token. Lua holds the view for the whole call, so the host reads the bytes in place while it runs and takes a reference of its own with `varn_dataview_acquire` to keep them longer.
- **The host to Lua.** A token in the answer of a host function, or in the payload of `varn_runtime_emit`, reaches Lua as a view. An emit takes its references when it is called, so the host releases its own view as soon as the emit returns, and an emit that names a view that is gone fails. A host function that made a view only for its answer hands it to `varn_runtime_release_dataview`, which gives the reference back once Lua holds its own.

```lua
local dataview = require("dataview")

-- The host reads the pixels in place while the call runs.
host.showFrame({ frame = dataview.from("\0\0\0\255") })

-- The host answers with a token, which arrives here as a view.
local captured = host.captureFrame()
print(captured.frame.size)
```

The [embedding guide](../embedding.md#sharing-memory-with-data-views) shows the C, C++, Objective-C, Swift, Java and JavaScript side of each direction.
