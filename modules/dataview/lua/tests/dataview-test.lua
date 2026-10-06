-- Covers making data views, reading and writing their bytes, the pointer that ffi takes, equality and releasing them.
local dataview = require("dataview")
local ffi = require("ffi")

-- A new view holds zeroed bytes.
local view = dataview.new(8)
assert(view.size == 8 and #view == 8, "A new view should have the size it was made with")
assert(view:read() == string.rep("\0", 8), "A new view should hold zeroed bytes")
assert(dataview.is(view) and not dataview.is("text"), 'The call "is" should recognize only data views')
assert(view.id > 0 and not view.released, "A live view should have an id and not be released")

-- Writes and reads count their offsets in bytes from zero.
view:write(2, "abc")
assert(view:read(2, 3) == "abc", "A read should see what a write put at the same offset")
assert(view:read(5) == "\0\0\0", "A read without a length should run to the end")

-- A view made from a string copies its bytes.
local copied = dataview.from("hello")
assert(copied:read() == "hello" and copied.size == 5, 'The call "from" should copy the bytes of the string')

-- Indexing reads and writes one byte in place, at an offset counted from zero.
local pixels = dataview.from("\10\20\30")
assert(pixels[0] == 10 and pixels[2] == 30, "Indexing should read a byte in place")
pixels[1] = 255
assert(pixels:read() == "\10\255\30", "An assignment at an offset should write the byte in place")
local outside, outsideMessage = pcall(function() return pixels[3] end)
assert(not outside and outsideMessage:find("The offset 3 lies outside the data view of 3 bytes.", 1, true), "An offset past the end should fail")
assert(not pcall(function() pixels[-1] = 0 end), "A negative offset should fail")
local wide, wideMessage = pcall(function() pixels[0] = 256 end)
assert(not wide and wideMessage:find("A byte of a data view holds an integer from 0 to 255, not 256.", 1, true), "A value that is not a byte should fail")
local named, namedMessage = pcall(function() pixels.size = 1 end)
assert(not named and namedMessage:find('A data view takes a byte at an offset, not the field "size".', 1, true), "A field should not be assignable")

-- The pointer reaches the same memory through ffi.
local bytes = ffi.cast("unsigned char *", view.pointer)
bytes[0] = 65
assert(view:read(0, 1) == "A", "A write through the pointer should be seen by a read")

-- A view equals another value only when both hold the same view.
assert(view == view and view ~= copied, "Views should be equal only to themselves")
assert(tostring(view) == "dataview (8 bytes)", "A view should describe its size")

-- Ranges outside the view are refused.
assert(not pcall(view.read, view, 7, 2), "A read past the end should fail")
assert(not pcall(view.write, view, 6, "abc"), "A write past the end should fail")
assert(not pcall(view.read, view, -1), "A negative offset should fail")
assert(not pcall(dataview.new, -1), "A negative size should fail")

-- A released view refuses every use but its state, and releasing it again does nothing.
view:release()
view:release()
assert(view.released, "A released view should say so")
assert(tostring(view) == "dataview (released)", "A released view should describe itself as released")
local ok, message = pcall(function() return view.size end)
assert(not ok and message:find("The data view was released."), "A released view should refuse its size")

-- A view of no bytes is valid.
local empty = dataview.new(0)
assert(empty.size == 0 and empty:read() == "", "A view of no bytes should read as an empty string")

-- Plain JSON never writes the address of a view.
local json = require("json")
assert(not pcall(json.encode, { frame = copied }), "The function json.encode should refuse a data view")

print("The \"dataview\" tests passed.")
