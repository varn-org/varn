-- A collected view is released in place, so a finalizer that reaches it later in the same cycle finds it released instead of freed memory.
local dataview = require("dataview")

local outcome = {}

-- The keeper is marked for finalization before the view, so its finalizer runs after the one of the view in the same cycle.
local keeper = setmetatable({}, {
    __gc = function(self)
        outcome.released = self.view.released
        outcome.text = tostring(self.view)
        outcome.ok, outcome.err = pcall(function() return self.view[0] end)
    end,
})

keeper.view = dataview.from("abc")
keeper = nil
collectgarbage("collect")
collectgarbage("collect")

assert(outcome.released == true, "A collected view should read as released")
assert(outcome.text == "dataview (released)", "A collected view should print as released, got " .. tostring(outcome.text))
assert(outcome.ok == false and tostring(outcome.err):find("The data view was released.", 1, true), "A collected view should refuse a read, got " .. tostring(outcome.err))

print("The \"dataview\" collection tests passed.")
