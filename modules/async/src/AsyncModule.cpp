#include "varn/async/AsyncModule.h"
#include "varn/async/Promise.h"
#include "varn/log/Log.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/EventLoop.h"
#include "varn/runtime/Runtime.h"

#include <memory>
#include <string>

namespace varn::async
{

using varn::runtime::Runtime;

static const char* const kFailureHandlerKey = "varn.taskFailure";

// A failure keeps at most this many frames, its innermost and its outermost halves around a marker of the ones left out, so a runaway recursion never builds a table as deep as its stack and still shows where it started.
static constexpr int kMaxFrames = 64;

// A Lua prelude attaching promise combinators on top of `spawn`, `await` and `deferred`, where each combinator waits on one deferred its inputs settle and a deadline cancels its timer once it is no longer needed.
static const char* const kCombinatorPrelude = R"lua(
local async, delay = ...

function async.all(list)
    return async.promise(function()
        local count = #list
        local results = {}
        local remaining = count
        local failure = nil
        local done, finish = async.deferred()

        if count == 0 then
            return results
        end

        for index = 1, count do
            local promise = list[index]
            async.spawn(function()
                local value, err = promise:await()
                if failure ~= nil or remaining == 0 then
                    return
                end

                if err ~= nil then
                    failure = err
                    finish()
                    return
                end

                results[index] = value
                remaining = remaining - 1
                if remaining == 0 then
                    finish()
                end
            end)
        end

        done:await()

        if failure ~= nil then
            error(failure, 0)
        end

        return results
    end)
end

function async.allSettled(list)
    return async.promise(function()
        local count = #list
        local results = {}
        local remaining = count
        local done, finish = async.deferred()

        if count == 0 then
            return results
        end

        for index = 1, count do
            local promise = list[index]
            async.spawn(function()
                local value, err = promise:await()
                if err ~= nil then
                    results[index] = { ok = false, error = err }
                else
                    results[index] = { ok = true, value = value }
                end

                remaining = remaining - 1
                if remaining == 0 then
                    finish()
                end
            end)
        end

        done:await()
        return results
    end)
end

function async.race(list)
    return async.promise(function()
        if #list == 0 then
            error('A race needs a promise, and "async.race" was given an empty list.', 0)
        end

        local settled = false
        local settledValue = nil
        local settledError = nil
        local done, finish = async.deferred()

        for index = 1, #list do
            local promise = list[index]
            async.spawn(function()
                local value, err = promise:await()
                if not settled then
                    settled = true
                    settledValue = value
                    settledError = err
                    finish()
                end
            end)
        end

        done:await()

        if settledError ~= nil then
            error(settledError, 0)
        end

        return settledValue
    end)
end

function async.any(list)
    return async.promise(function()
        local count = #list
        local resolved = false
        local resolvedValue = nil
        local remaining = count
        local lastError = nil
        local done, finish = async.deferred()

        if count == 0 then
            error('Every promise given to "async.any" rejected.', 0)
        end

        for index = 1, count do
            local promise = list[index]
            async.spawn(function()
                local value, err = promise:await()
                if resolved then
                    return
                end

                if err ~= nil then
                    lastError = err
                else
                    resolved = true
                    resolvedValue = value
                    finish()
                    return
                end

                remaining = remaining - 1
                if remaining == 0 then
                    finish()
                end
            end)
        end

        done:await()

        if resolved then
            return resolvedValue
        end

        error(lastError or 'Every promise given to "async.any" rejected.', 0)
    end)
end

function async.timeout(promise, ms)
    return async.promise(function()
        local settled = false
        local timedOut = false
        local value = nil
        local failure = nil
        local done, finish = async.deferred()
        local elapsed, cancel = delay(ms)

        async.spawn(function()
            local fired = elapsed:await()
            if fired and not settled then
                settled = true
                timedOut = true
                finish()
            end
        end)

        async.spawn(function()
            local result, err = promise:await()
            if not settled then
                settled = true
                value = result
                failure = err
                cancel()
                finish()
            end
        end)

        done:await()

        if timedOut then
            error('The promise given to "async.timeout" did not settle within ' .. tostring(ms) .. " milliseconds.", 0)
        end

        if failure ~= nil then
            error(failure, 0)
        end

        return value
    end)
end

function async.mapLimit(list, limit, fn)
    if type(limit) ~= "number" or limit < 1 then
        error('The limit given to "async.mapLimit" must be a number that is at least 1.', 0)
    end
    return async.promise(function()
        local count = #list
        local results = {}
        local nextIndex = 1
        local inFlight = 0
        local completed = 0
        local failed = false
        local failure = nil
        local pumping = false
        local done, finish = async.deferred()

        if count == 0 then
            return results
        end

        -- Keeps at most `limit` calls in flight, and a call that settles at once hands its place back to the loop already running rather than starting one inside it.
        local function pump()
            if pumping then
                return
            end

            pumping = true
            while not failed and nextIndex <= count and inFlight < limit do
                local index = nextIndex
                nextIndex = nextIndex + 1
                inFlight = inFlight + 1
                async.spawn(function()
                    -- Guard the mapper so a throwing `fn` or a non-promise result still settles the counters instead of hanging the whole map.
                    local ok, value, err = pcall(function()
                        return fn(list[index]):await()
                    end)
                    inFlight = inFlight - 1
                    if failed then
                        return
                    end

                    if not ok or err ~= nil then
                        failed = true
                        failure = not ok and value or err
                        finish()
                        return
                    end

                    results[index] = value
                    completed = completed + 1
                    if completed == count then
                        finish()
                        return
                    end

                    pump()
                end)
            end
            pumping = false
        end

        pump()
        done:await()

        if failed then
            error(failure, 0)
        end

        return results
    end)
end
)lua";

Runtime& AsyncModule::luaRuntime(lua_State* L)
{
    return *static_cast<Runtime*>(varn::lua::LuaHelpers::getRuntime(L));
}

int AsyncModule::luaSleep(lua_State* L)
{
    auto& rt = luaRuntime(L);
    const lua_Integer requested = luaL_checkinteger(L, 1);
    const long long ms = requested > 0 ? static_cast<long long>(requested) : 0;
    auto promise = std::make_shared<Promise>(rt);

    // Waiting is a timer on the loop, on every target including the browser.
    // Under wasm the wait used to be an `emscripten_sleep`, which unwinds the whole wasm stack and rewinds it from a callback of its own.
    // A host that drives the runtime from its own pump — which is what a user interface does — calls back in while that stack is still unwound, and the rewind then reads a stack nothing holds any more: the module dies with "memory access out of bounds" and every call after it dies the same way.
    // A page dismissing an overlay hit it every time, since a thing leaving the screen is held for as long as it takes to leave.
    rt.mainLoop().postDelayed(ms, [promise]
                              { promise->resolve("ok"); });

    Promise::push(L, promise);
    return 1;
}

// A timer the prelude races against work, answering true once it fires and false once the work settled first and cancelled it.
int AsyncModule::luaDelay(lua_State* L)
{
    auto& rt = luaRuntime(L);
    const lua_Integer requested = luaL_checkinteger(L, 1);
    const long long ms = requested > 0 ? static_cast<long long>(requested) : 0;
    auto promise = std::make_shared<Promise>(rt);

    // clang-format off
    const auto timer = rt.mainLoop().postDelayed(ms, [promise]
    {
        promise->resolveCustom([](lua_State* target) { lua_pushboolean(target, 1); });
    });
    // clang-format on

    Promise::push(L, promise);
    lua_pushvalue(L, -1);
    lua_pushinteger(L, static_cast<lua_Integer>(timer));
    lua_pushcclosure(L, &AsyncModule::luaCancelDelay, 2);
    return 2;
}

// Cancelling removes the timer from the loop and settles its promise at once, so neither a timer nor a coroutine waiting for it is left behind.
int AsyncModule::luaCancelDelay(lua_State* L)
{
    Promise* promise = Promise::check(L, lua_upvalueindex(1));
    const auto timer = static_cast<varn::runtime::EventLoop::TimerId>(lua_tointeger(L, lua_upvalueindex(2)));

    if (luaRuntime(L).mainLoop().cancelTimer(timer))
    {
        promise->resolveCustom([](lua_State* target)
                               { lua_pushboolean(target, 0); });
    }

    return 0;
}

int AsyncModule::luaCancelTask(lua_State* L)
{
    Promise::cancelTask(L, lua_upvalueindex(1));
    return 0;
}

// Sets the one handler that receives the error and the traceback of any task of `spawn` or `run` that fails, or clears it with `nil`.
int AsyncModule::luaOnFailure(lua_State* L)
{
    if (!lua_isnoneornil(L, 1))
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
    }

    lua_settop(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, kFailureHandlerKey);
    return 0;
}

// Keeps an error as it was raised beside the traceback and the frames of where it was raised, which the stack no longer holds once the protected call returns, as a message handler of a protected call.
int AsyncModule::capture(lua_State* L)
{
    lua_createtable(L, 0, 3);
    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "error");
    luaL_traceback(L, L, nullptr, 1);
    lua_setfield(L, -2, "traceback");

    // Each frame names its source, its line, its function and its kind, from the innermost out, so a host shows them without reading the traceback back.
    const int depth = stackDepth(L);
    const int kept = depth > kMaxFrames ? kMaxFrames / 2 : depth;
    lua_createtable(L, depth > kMaxFrames ? kMaxFrames + 1 : depth, 0);
    int index = 0;

    for (int level = 1; level <= kept; ++level)
    {
        pushFrame(L, level);
        lua_rawseti(L, -2, ++index);
    }

    if (depth > kMaxFrames)
    {
        lua_createtable(L, 0, 2);
        lua_pushliteral(L, "skipped");
        lua_setfield(L, -2, "kind");
        lua_pushinteger(L, depth - kMaxFrames);
        lua_setfield(L, -2, "count");
        lua_rawseti(L, -2, ++index);

        for (int level = depth - kMaxFrames / 2 + 1; level <= depth; ++level)
        {
            pushFrame(L, level);
            lua_rawseti(L, -2, ++index);
        }
    }

    lua_setfield(L, -2, "frames");
    return 1;
}

// Answers how many levels the stack holds above the message handler, found by doubling and then halving so a deep stack is never walked level by level.
int AsyncModule::stackDepth(lua_State* L)
{
    lua_Debug frame;
    int known = 1;
    int beyond = 1;

    while (lua_getstack(L, beyond, &frame) != 0)
    {
        known = beyond;
        beyond *= 2;
    }

    while (known < beyond)
    {
        const int middle = (known + beyond) / 2;
        if (lua_getstack(L, middle, &frame) != 0)
        {
            known = middle + 1;
        }
        else
        {
            beyond = middle;
        }
    }

    return beyond - 1;
}

// Pushes one frame of the stack as a table of its source, line, name, the way it was named, where its function was defined and its kind.
void AsyncModule::pushFrame(lua_State* L, int level)
{
    lua_Debug frame;
    lua_getstack(L, level, &frame);
    lua_getinfo(L, "Sln", &frame);

    lua_createtable(L, 0, 6);
    lua_pushstring(L, frame.short_src);
    lua_setfield(L, -2, "source");
    lua_pushstring(L, frame.what);
    lua_setfield(L, -2, "kind");

    if (frame.currentline > 0)
    {
        lua_pushinteger(L, frame.currentline);
        lua_setfield(L, -2, "line");
    }

    if (frame.linedefined > 0)
    {
        lua_pushinteger(L, frame.linedefined);
        lua_setfield(L, -2, "linedefined");
    }

    if (frame.name != nullptr)
    {
        lua_pushstring(L, frame.name);
        lua_setfield(L, -2, "name");
    }

    if (frame.namewhat != nullptr && frame.namewhat[0] != '\0')
    {
        lua_pushstring(L, frame.namewhat);
        lua_setfield(L, -2, "namewhat");
    }
}

// Hands a failure to the handler the application set and answers whether it took it, and a handler that fails leaves the failure to the runtime.
bool AsyncModule::reportFailure(lua_State* L)
{
    if (lua_getfield(L, LUA_REGISTRYINDEX, kFailureHandlerKey) != LUA_TFUNCTION)
    {
        lua_pop(L, 1);
        return false;
    }

    lua_getfield(L, -2, "error");
    lua_getfield(L, -3, "traceback");
    lua_getfield(L, -4, "frames");

    if (lua_pcall(L, 3, 0, 0) != LUA_OK)
    {
        log::Log::error("Runtime", std::string("The failure handler of the tasks failed: ") + luaL_tolstring(L, -1, nullptr));
        lua_pop(L, 2);
        return false;
    }

    return true;
}

// Hands the failure at the top of the stack, the table "capture" builds or a bare error no capture kept, to the handler the application set, or else to the runtime as an unhandled failure that stops the loop, and pops it.
bool AsyncModule::fail(lua_State* L)
{
    // A bare error is carried in the same shape, so the handler receives it too, without a traceback.
    if (!lua_istable(L, -1))
    {
        lua_createtable(L, 0, 1);
        lua_insert(L, -2);
        lua_setfield(L, -2, "error");
    }

    if (reportFailure(L))
    {
        lua_pop(L, 1);
        return true;
    }

    lua_getfield(L, -1, "error");
    lua_getfield(L, -2, "traceback");
    const char* error = lua_tostring(L, -2);
    const char* traceback = lua_tostring(L, -1);
    const std::string message = std::string(error ? error : "The failure carries no message.") + (traceback ? std::string("\n") + traceback : std::string());
    lua_pop(L, 3);

    luaRuntime(L).onAsyncComplete(false, false, message);
    return false;
}

int AsyncModule::entryContinuation(lua_State* L, int status, lua_KContext ctx)
{
    const bool stopLoopOnSuccess = ctx != 0;

    if (status != LUA_OK && status != LUA_YIELD && !fail(L))
    {
        return 0;
    }

    luaRuntime(L).onAsyncComplete(true, stopLoopOnSuccess, std::string());
    return 0;
}

int AsyncModule::entryBody(lua_State* L)
{
    const lua_KContext ctx = lua_toboolean(L, lua_upvalueindex(2)) ? 1 : 0;

    // Runs the user function under a protected call that survives await yields, with a handler that keeps the traceback of a failure.
    lua_pushcfunction(L, &AsyncModule::capture);
    lua_pushvalue(L, lua_upvalueindex(1));
    const int status = lua_pcallk(L, 0, 0, lua_gettop(L) - 1, ctx, &AsyncModule::entryContinuation);
    return entryContinuation(L, status, ctx);
}

// Starts a task at once and answers its handle, whose `cancel` stops it at its next await for good.
int AsyncModule::startEntry(lua_State* L, bool stopLoopOnSuccess)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    lua_State* thread = lua_newthread(L);
    lua_pushvalue(L, -1);
    const int threadRef = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_pushvalue(L, 1);
    lua_xmove(L, thread, 1);
    lua_pushboolean(thread, stopLoopOnSuccess ? 1 : 0);
    lua_pushcclosure(thread, &AsyncModule::entryBody, 2);

#if defined(__EMSCRIPTEN__)
    lua_sethook(thread, nullptr, 0, 0);
#endif

    int nres = 0;
    const int status = lua_resume(thread, L, 0, &nres);
    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* raw = lua_tostring(thread, -1);
        const std::string message = raw ? raw : "[AsyncModule] The task could not be started.";
        luaL_unref(L, LUA_REGISTRYINDEX, threadRef);
        return luaL_error(L, "%s", message.c_str());
    }

    // Releases the thread ref held during startup, and the handle keeps the thread for as long as it lives.
    luaL_unref(L, LUA_REGISTRYINDEX, threadRef);
    lua_createtable(L, 0, 1);
    lua_insert(L, -2);
    lua_pushcclosure(L, &AsyncModule::luaCancelTask, 1);
    lua_setfield(L, -2, "cancel");
    return 1;
}

int AsyncModule::promiseContinuation(lua_State* L, int status, lua_KContext ctx)
{
    (void)ctx;
    auto& rt = luaRuntime(L);

    // Reads the promise userdata from the coroutine upvalue.
    lua_pushvalue(L, lua_upvalueindex(2));
    Promise* promise = Promise::check(L, -1);
    lua_pop(L, 1);

    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* message = lua_tostring(L, -1);
        promise->reject(message ? message : "The task of \"async.promise\" failed without a message.");
        return 0;
    }

    // Captures the function result in the registry and unrefs it when the promise is gone.
    lua_pushvalue(L, -1);
    const int valueRef = luaL_ref(L, LUA_REGISTRYINDEX);
    Runtime* runtime = &rt;

    // clang-format off
    std::shared_ptr<int> refHolder(new int(valueRef), [runtime](int* ref)
    {
        if (!runtime->stopped())
        {
            luaL_unref(runtime->luaState(), LUA_REGISTRYINDEX, *ref);
        }

        delete ref;
    });
    // clang-format on

    promise->resolveCustom([refHolder](lua_State* target)
                           { lua_rawgeti(target, LUA_REGISTRYINDEX, *refHolder); });

    return 0;
}

int AsyncModule::promiseBody(lua_State* L)
{
    // Runs the user function under a protected call that survives await yields.
    lua_pushvalue(L, lua_upvalueindex(1));
    const int status = lua_pcallk(L, 0, 1, 0, 0, &AsyncModule::promiseContinuation);
    return promiseContinuation(L, status, 0);
}

int AsyncModule::luaPromise(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    auto& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);

    lua_State* thread = lua_newthread(L);
    const int threadRef = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_pushvalue(L, 1);
    lua_xmove(L, thread, 1);
    Promise::push(thread, promise);
    lua_pushcclosure(thread, &AsyncModule::promiseBody, 2);

#if defined(__EMSCRIPTEN__)
    lua_sethook(thread, nullptr, 0, 0);
#endif

    int nres = 0;
    const int status = lua_resume(thread, L, 0, &nres);
    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* raw = lua_tostring(thread, -1);
        const std::string message = raw ? raw : "The task of \"async.promise\" could not be started.";
        luaL_unref(L, LUA_REGISTRYINDEX, threadRef);
        return luaL_error(L, "%s", message.c_str());
    }

    // Releases the thread ref held during startup.
    luaL_unref(L, LUA_REGISTRYINDEX, threadRef);

    Promise::push(L, promise);
    return 1;
}

namespace
{
constexpr const char* kResolverGuardMeta = "varn.ResolverGuard";

struct ResolverGuardUserdata
{
    std::shared_ptr<Promise> promise;
};
} // namespace

int AsyncModule::luaResolveDeferred(lua_State* L)
{
    Promise* promise = Promise::check(L, lua_upvalueindex(1));
    promise->resolve("ok");
    return 0;
}

int AsyncModule::luaResolverGc(lua_State* L)
{
    auto* guard = static_cast<ResolverGuardUserdata*>(luaL_checkudata(L, 1, kResolverGuardMeta));

    // The resolver closure that held this guard was collected without resolving, so break the promise to resume and release its awaiters.
    if (guard->promise)
    {
        guard->promise->breakIfPending();
    }

    guard->promise.~shared_ptr<Promise>();
    return 0;
}

void AsyncModule::installResolverMetatable(lua_State* L)
{
    if (luaL_newmetatable(L, kResolverGuardMeta) == 0)
    {
        lua_pop(L, 1);
        return;
    }

    lua_pushcfunction(L, &AsyncModule::luaResolverGc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

int AsyncModule::luaDeferred(lua_State* L)
{
    auto& rt = luaRuntime(L);
    auto promise = std::make_shared<Promise>(rt);

    // The awaitable is returned alongside a one-shot resolve function.
    Promise::push(L, promise);

    // A finalizable guard rides along as an upvalue of the resolve closure so dropping the resolver breaks a still-pending promise.
    void* memory = lua_newuserdatauv(L, sizeof(ResolverGuardUserdata), 0);
    new (memory) ResolverGuardUserdata{promise};
    luaL_getmetatable(L, kResolverGuardMeta);
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -2);
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, &AsyncModule::luaResolveDeferred, 2);

    // Drop the bare guard now that the closure owns it, leaving the awaitable and the resolve function on the stack.
    lua_remove(L, -2);
    return 2;
}

int AsyncModule::luaSpawn(lua_State* L)
{
    return startEntry(L, false);
}

int AsyncModule::luaRun(lua_State* L)
{
    return startEntry(L, true);
}

int AsyncModule::luaOpen(lua_State* L)
{
    installResolverMetatable(L);

    lua_newtable(L);

    lua_pushcfunction(L, &AsyncModule::luaSleep);
    lua_setfield(L, -2, "sleep");

    lua_pushcfunction(L, &AsyncModule::luaSpawn);
    lua_setfield(L, -2, "spawn");

    lua_pushcfunction(L, &AsyncModule::luaRun);
    lua_setfield(L, -2, "run");

    lua_pushcfunction(L, &AsyncModule::luaPromise);
    lua_setfield(L, -2, "promise");

    lua_pushcfunction(L, &AsyncModule::luaDeferred);
    lua_setfield(L, -2, "deferred");

    lua_pushcfunction(L, &AsyncModule::luaOnFailure);
    lua_setfield(L, -2, "onFailure");

    installCombinators(L);

    return 1;
}

void AsyncModule::installCombinators(lua_State* L)
{
    if (luaL_loadstring(L, kCombinatorPrelude) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        luaL_error(L, "[AsyncModule] The combinator prelude failed to compile: %s", message ? message : "");
    }

    // Passes the module table on top of the stack to the prelude, with the cancellable timer only the prelude reaches.
    lua_pushvalue(L, -2);
    lua_pushcfunction(L, &AsyncModule::luaDelay);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        luaL_error(L, "[AsyncModule] The combinator prelude failed to run: %s", message ? message : "");
    }
}

void AsyncModule::install(lua_State* L)
{
    luaL_requiref(L, "async", &AsyncModule::luaOpen, 1);
    lua_pop(L, 1);
}

} // namespace varn::async
