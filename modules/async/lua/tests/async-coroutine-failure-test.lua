-- A plain coroutine that awaited a promise and then fails has no caller left, so its failure reaches the failure handler with its traceback.
local async = require("async")

local received = nil
async.onFailure(function(err, traceback, frames, kind)
    received = { err = err, traceback = traceback, kind = kind }
end)

local plain = coroutine.create(function()
    async.sleep(1):await()
    error("The plain coroutine failed.", 0)
end)
assert(coroutine.resume(plain), "The plain coroutine should start and wait.")

async.run(function()
    async.sleep(30):await()
    async.onFailure(nil)

    assert(received ~= nil, "The failure handler should receive the failure of the plain coroutine.")
    assert(received.err == "The plain coroutine failed.", "The handler should receive the error as it was raised.")
    assert(received.kind == "failure", "The failure should be of the kind \"failure\".")
    assert(type(received.traceback) == "string" and received.traceback:find("stack traceback", 1, true), "The failure should carry the traceback of the coroutine.")
    print('The "async" coroutine failure tests passed.')
end)
