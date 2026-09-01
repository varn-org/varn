#include "varn/async/AsyncModule.h"
#include "varn/async/Promise.h"
#include "varn/lua/LuaHelpers.h"
#include "varn/runtime/Runtime.h"

#include <memory>
#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace varn::async
{

using varn::runtime::Runtime;

// lua prelude attaching promise combinators on top of sleep/spawn/await/promise
static const char* const kCombinatorPrelude = R"lua(
local async = ...

-- yields the current coroutine until predicate() returns true, polling on the loop
local function waitUntil(predicate)
    while not predicate() do
        async.sleep(1):await()
    end
end

function async.all(list)
    return async.promise(function()
        local count = #list
        local results = {}
        local remaining = count
        local failed = false
        local failure = nil

        for index = 1, count do
            local promise = list[index]
            async.spawn(function()
                local value, err = promise:await()
                if err ~= nil then
                    if not failed then
                        failed = true
                        failure = err
                    end
                else
                    results[index] = value
                end
                remaining = remaining - 1
            end)
        end

        waitUntil(function() return failed or remaining == 0 end)

        if failed then
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
            end)
        end

        waitUntil(function() return remaining == 0 end)

        return results
    end)
end

function async.race(list)
    return async.promise(function()
        if #list == 0 then
            error("async.race: cannot race an empty list", 0)
        end

        local settled = false
        local settledValue = nil
        local settledError = nil

        for index = 1, #list do
            local promise = list[index]
            async.spawn(function()
                local value, err = promise:await()
                if not settled then
                    settled = true
                    settledValue = value
                    settledError = err
                end
            end)
        end

        waitUntil(function() return settled end)

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

        for index = 1, count do
            local promise = list[index]
            async.spawn(function()
                local value, err = promise:await()
                if err ~= nil then
                    lastError = err
                elseif not resolved then
                    resolved = true
                    resolvedValue = value
                end
                remaining = remaining - 1
            end)
        end

        waitUntil(function() return resolved or remaining == 0 end)

        if resolved then
            return resolvedValue
        end

        error(lastError or "async.any: all promises rejected", 0)
    end)
end

function async.timeout(promise, ms)
    return async.promise(function()
        local settled = false
        local timedOut = false
        local value = nil
        local failure = nil

        async.spawn(function()
            local result, err = promise:await()
            if not settled then
                settled = true
                value = result
                failure = err
            end
        end)

        async.spawn(function()
            async.sleep(ms):await()
            if not settled then
                settled = true
                timedOut = true
            end
        end)

        waitUntil(function() return settled end)

        if timedOut then
            error("async.timeout: promise did not settle within " .. tostring(ms) .. "ms", 0)
        end

        if failure ~= nil then
            error(failure, 0)
        end

        return value
    end)
end

function async.mapLimit(list, limit, fn)
    if type(limit) ~= "number" or limit < 1 then
        error("async.mapLimit: limit must be a number that is at least 1", 0)
    end
    return async.promise(function()
        local count = #list
        local results = {}
        local nextIndex = 1
        local inFlight = 0
        local completed = 0
        local failed = false
        local failure = nil

        local function startNext()
            local index = nextIndex
            nextIndex = nextIndex + 1
            inFlight = inFlight + 1
            async.spawn(function()
                -- guard the mapper so a throwing fn or a non-promise result still settles the counters instead of hanging the whole map
                local ok, value, err = pcall(function()
                    return fn(list[index]):await()
                end)
                if not ok then
                    if not failed then
                        failed = true
                        failure = value
                    end
                elseif err ~= nil then
                    if not failed then
                        failed = true
                        failure = err
                    end
                else
                    results[index] = value
                end
                inFlight = inFlight - 1
                completed = completed + 1
            end)
        end

        while not failed and nextIndex <= count and inFlight < limit do
            startNext()
        end

        while completed < count and not failed do
            async.sleep(1):await()
            while not failed and nextIndex <= count and inFlight < limit do
                startNext()
            end
        end

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

#if defined(__EMSCRIPTEN__)
    // clang-format off
    rt.taskPool().post([promise, ms]
    {
        emscripten_sleep(static_cast<unsigned int>(ms));
        promise->resolve("ok");
    });
    // clang-format on
#else
    // schedules the resolve on the loop timer
    rt.mainLoop().postDelayed(ms, [promise]
                              { promise->resolve("ok"); });
#endif

    Promise::push(L, promise);
    return 1;
}

int AsyncModule::entryContinuation(lua_State* L, int status, lua_KContext ctx)
{
    const bool stopLoopOnSuccess = ctx != 0;
    auto& rt = luaRuntime(L);

    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* message = lua_tostring(L, -1);
        rt.onAsyncComplete(false, stopLoopOnSuccess, message ? message : "");
        return 0;
    }

    rt.onAsyncComplete(true, stopLoopOnSuccess, std::string());
    return 0;
}

int AsyncModule::entryBody(lua_State* L)
{
    const lua_KContext ctx = lua_toboolean(L, lua_upvalueindex(2)) ? 1 : 0;

    // runs the user function under a protected call that survives await yields
    lua_pushvalue(L, lua_upvalueindex(1));
    const int status = lua_pcallk(L, 0, 0, 0, ctx, &AsyncModule::entryContinuation);
    return entryContinuation(L, status, ctx);
}

int AsyncModule::startEntry(lua_State* L, bool stopLoopOnSuccess)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    lua_State* thread = lua_newthread(L);
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

    // releases the thread ref held during startup
    luaL_unref(L, LUA_REGISTRYINDEX, threadRef);
    return 0;
}

int AsyncModule::promiseContinuation(lua_State* L, int status, lua_KContext ctx)
{
    (void)ctx;
    auto& rt = luaRuntime(L);

    // reads the promise userdata from the coroutine upvalue
    lua_pushvalue(L, lua_upvalueindex(2));
    Promise* promise = Promise::check(L, -1);
    lua_pop(L, 1);

    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* message = lua_tostring(L, -1);
        promise->reject(message ? message : "async.promise: the task failed without a message.");
        return 0;
    }

    // captures the function result in the registry and unrefs it when the promise is gone
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
    // runs the user function under a protected call that survives await yields
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
        const std::string message = raw ? raw : "async.promise: the task could not be started.";
        luaL_unref(L, LUA_REGISTRYINDEX, threadRef);
        return luaL_error(L, "%s", message.c_str());
    }

    // releases the thread ref held during startup
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

    // the resolver closure that held this guard was collected without resolving, so break the promise to resume and release its awaiters
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

    // the awaitable is returned alongside a one-shot resolve function
    Promise::push(L, promise);

    // a finalizable guard rides along as an upvalue of the resolve closure so dropping the resolver breaks a still-pending promise
    void* memory = lua_newuserdatauv(L, sizeof(ResolverGuardUserdata), 0);
    new (memory) ResolverGuardUserdata{promise};
    luaL_getmetatable(L, kResolverGuardMeta);
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -2);
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, &AsyncModule::luaResolveDeferred, 2);

    // drop the bare guard now that the closure owns it, leaving the awaitable and the resolve function on the stack
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

    // passes the module table on top of the stack to the prelude as its single argument
    lua_pushvalue(L, -2);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
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
