# 🪟 dataview

Memory that Lua and the host share by reference instead of copying it. A data view is an address, a size and an id. Lua, C, C++, Objective-C, Swift, Java, Kotlin and JavaScript each hold a reference to the same bytes, so a large buffer such as a camera frame crosses between them without a copy and without turning into text.

```lua
local dataview = require("dataview")
local frame = dataview.new(1280 * 720 * 4)
frame:write(0, "\255\0\0\255")
print(frame.size, frame:read(0, 4) == "\255\0\0\255")
```

## Capabilities

| Function | What it does |
|---|---|
| `dataview.new(size)` | A view of `size` zeroed bytes. |
| `dataview.from(text)` | A view that holds a copy of the bytes of a string. |
| `dataview.is(value)` | Whether a value is a data view. |
| `view.size` / `#view` | The size in bytes. |
| `view.pointer` | The address as a light userdata, which `ffi.cast` turns into a pointer. |
| `view.id` | The id of the view, which no other view of the process ever takes. |
| `view[offset]` | Reads or writes one byte in place, counted from zero, with no copy. |
| `view:read(offset?, length?)` | A string with a copy of a range, counted in bytes from zero. |
| `view:write(offset, text)` | Copies the bytes of a string into the view at `offset`. |
| `view:release()` / `view.released` | Drops the reference of this value now, and whether it was dropped. |

A view reaches a host function and comes back from one, and from an event the host emits, as a token in the JSON of the bridge. The C API (`varn_dataview_*` in `varn.h`), the C++ class `varn::dataview::DataView`, the Objective-C class `VarnDataView`, the Java class `com.varn.VarnDataView` and the JavaScript class `VarnDataView` of the browser build hold the same views.

## Reference and tests

- Full reference: [docs/lua-api/dataview.md](../../docs/lua-api/dataview.md)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/) and [tests/cpp/dataview-test.cpp](../../tests/cpp/dataview-test.cpp)
