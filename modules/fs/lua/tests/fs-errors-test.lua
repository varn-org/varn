-- A failure of the file system reaches Lua as a sentence that gives its reason, and a collected handle refuses every call.
local async = require("async")
local fs = require("fs")
local platform = require("platform")

local dir = assert(os.getenv("VARN_TEST_DIR"), 'The "VARN_TEST_DIR" variable is not set. Run the tests with "python3 varn.py test".')

-- A name longer than any component the system allows cannot be checked, which is an error rather than a missing file.
if platform.os() ~= "windows" then
    local ok, err = pcall(fs.exists, dir .. "/" .. ("a"):rep(5000))
    assert(not ok, 'The call "exists" on a name that is too long should raise')
    assert(tostring(err):find("The path could not be checked", 1, true), "The error should say what failed, got " .. tostring(err):sub(1, 200))
end

assert(fs.exists(dir), 'The call "exists" should find the scratch folder')
assert(not fs.exists(dir .. "/fs-errors-missing"), 'The call "exists" should answer false for a missing path')

async.run(function()
    -- A folder that cannot be created under a regular file fails with a sentence for every call that creates the folders of its path.
    local blocker = dir .. "/fs-errors-blocker"
    fs.writeFile(blocker, "x"):await()
    local inside = blocker .. "/sub/file.txt"
    for name, call in pairs({
        writeFile = function() return fs.writeFile(inside, "x") end,
        append = function() return fs.append(inside, "x") end,
        open = function() return fs.open(inside, "w") end,
        mkdtemp = function() return fs.mkdtemp(blocker .. "/sub/tmp-") end,
    }) do
        local _, err = call():await()
        assert(err and tostring(err):find("The folder could not be created: ", 1, true), 'The call "' .. name .. '" should reject with a sentence, got ' .. tostring(err))
    end

    -- The keeper is marked for finalization before the handle, so its finalizer runs after the one of the handle in the same cycle.
    local outcome = {}
    local keeper = setmetatable({}, {
        __gc = function(self)
            outcome.ok, outcome.err = pcall(self.handle.write, self.handle, "late")
        end,
    })

    keeper.handle = fs.open(dir .. "/fs-errors-handle.txt", "w"):await()
    keeper = nil
    collectgarbage("collect")
    collectgarbage("collect")

    assert(outcome.ok == false, "A collected handle should refuse a call from a later finalizer")
    assert(tostring(outcome.err):find("The file handle was already collected.", 1, true), "The error should name the collected handle, got " .. tostring(outcome.err))

    print("The \"fs\" error tests passed.")
end)
