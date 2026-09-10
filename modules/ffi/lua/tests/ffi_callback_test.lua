-- Raises the failure of a callback from the call it ran under, with the traceback of the callback, and runs every callback on the main thread of Lua.
local ffi = require("ffi")

ffi.cdef [[
    void qsort(void *base, unsigned long count, unsigned long size, int (*compare)(const void *, const void *));
]]

local function fill(values)
    for index, value in ipairs({ 4, 1, 3, 2 }) do
        values[index - 1] = value
    end
end

local function ascending(a, b)
    local left = ffi.cast("const int *", a)[0]
    local right = ffi.cast("const int *", b)[0]
    return left < right and -1 or (left > right and 1 or 0)
end

local values = ffi.new("int[4]")
fill(values)

-- A comparator that raises fails the call to "qsort", whose message carries the error and the traceback of the comparator.
local failing = ffi.cast("int (*)(const void *, const void *)", function()
    error("The comparator gave up.")
end)

local ok, err = pcall(ffi.C.qsort, values, 4, ffi.sizeof("int"), failing)
assert(not ok, 'The call "qsort" should fail with its comparator.')
assert(err:find("The comparator gave up.", 1, true), "The failure should carry the error of the comparator, not: " .. tostring(err))

local traceback = err:find("stack traceback:", 1, true)
assert(traceback and traceback > err:find("The comparator gave up.", 1, true), "The traceback should follow the error, not: " .. tostring(err))
assert(err:find("ffi_callback_test.lua:25:", traceback, true), "The traceback should reach the line of the comparator, not: " .. tostring(err))

-- The failure goes with the call that raised it, so the same callback serves a later call.
local fail_next = true
local flaky = ffi.cast("int (*)(const void *, const void *)", function(a, b)
    if fail_next then
        fail_next = false
        error("Only once.")
    end
    return ascending(a, b)
end)

assert(not pcall(ffi.C.qsort, values, 4, ffi.sizeof("int"), flaky), 'The first call "qsort" should fail.')
fill(values)
ffi.C.qsort(values, 4, ffi.sizeof("int"), flaky)
assert(values[0] == 1 and values[3] == 4, 'The second call "qsort" should sort with the same callback.')

-- An answer that does not convert fails the call and names the type of the callback.
local wrong = ffi.cast("int (*)(const void *, const void *)", function()
    return "not a number"
end)

local ok_wrong, err_wrong = pcall(ffi.C.qsort, values, 4, ffi.sizeof("int"), wrong)
assert(not ok_wrong, "An answer that does not convert should fail the call.")
assert(err_wrong:find('"int (*)(const void *, const void *)"', 1, true), "The failure should name the callback type, not: " .. tostring(err_wrong))
assert(err_wrong:find('"string"', 1, true), "The failure should name what the callback returned, not: " .. tostring(err_wrong))

-- A Lua function passed where a function pointer is expected is refused, since only a cast keeps the callback alive.
local ok_direct, err_direct = pcall(ffi.C.qsort, values, 4, ffi.sizeof("int"), ascending)
assert(not ok_direct and err_direct:find('"ffi.cast"', 1, true), "A Lua function passed directly should be refused, not: " .. tostring(err_direct))

-- A callback made inside a coroutine runs on the main thread, so it outlives the coroutine.
local ran_on_main = nil
local maker = coroutine.create(function()
    return ffi.cast("int (*)(const void *, const void *)", function(a, b)
        local _, is_main = coroutine.running()
        ran_on_main = is_main
        return ascending(a, b)
    end)
end)

local made, from_coroutine = coroutine.resume(maker)
assert(made and coroutine.status(maker) == "dead", "The coroutine should make the callback and end.")

fill(values)
ffi.C.qsort(values, 4, ffi.sizeof("int"), from_coroutine)
assert(ran_on_main == true, "A callback made inside a coroutine should run on the main thread.")
assert(values[0] == 1 and values[3] == 4, "A callback made inside a coroutine should still sort.")

-- A call made from inside a coroutine runs the callback on the main thread too and raises its failure in the coroutine.
ran_on_main = nil
fill(values)
coroutine.wrap(function()
    ffi.C.qsort(values, 4, ffi.sizeof("int"), from_coroutine)
end)()
assert(ran_on_main == true, "A callback called from a coroutine should run on the main thread.")

local ok_inner, err_inner = coroutine.wrap(function()
    return pcall(ffi.C.qsort, values, 4, ffi.sizeof("int"), failing)
end)()
assert(not ok_inner and err_inner:find("The comparator gave up.", 1, true), "The coroutine should see the failure of the callback, not: " .. tostring(err_inner))

print("The \"ffi\" callback tests passed.")
