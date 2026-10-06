-- Argument-type error paths for the async entry points and the `sleep` non-positive clamp.
local async = require("async")

-- The `spawn`, `run` and `promise` calls reject a non-function argument.
assert(not pcall(function() return async.spawn(42) end), 'The "spawn" call should reject a non-function.')
assert(not pcall(function() return async.run(42) end), 'The "run" call should reject a non-function.')
assert(not pcall(function() return async.promise("x") end), 'The "promise" call should reject a non-function.')

-- The `sleep` call rejects a non-number delay.
assert(not pcall(function() return async.sleep("soon") end), 'The "sleep" call should reject a non-number delay.')

-- The combinators refuse at once anything but promises, which they could never await.
for _, name in ipairs({ "all", "allSettled", "race", "any" }) do
    local ok, err = pcall(async[name], { async.sleep(1), 42 })
    assert(not ok and err:find('"async.' .. name .. '" takes promises', 1, true), 'The "' .. name .. '" combinator should refuse a number in its list.')
    ok, err = pcall(async[name], "list")
    assert(not ok and err:find("takes a list of promises", 1, true), 'The "' .. name .. '" combinator should refuse a string for its list.')
end

do
    local ok, err = pcall(async.timeout, {}, 10)
    assert(not ok and err:find('"async.timeout" takes promises', 1, true), 'The "timeout" combinator should refuse a table.')
end

-- A non-positive sleep clamps to zero and still resolves promptly.
async.run(function()
    async.sleep(0):await()
    async.sleep(-5):await()
    print('The "async" error tests passed.')
end)
