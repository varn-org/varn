#include "varn/async/Promise.h"
#include "varn/async/AsyncModule.h"
#include "varn/log/Log.h"
#include "varn/runtime/Runtime.h"

#include <string>
#include <tuple>

namespace varn::async
{

using varn::runtime::Runtime;

constexpr const char* PromiseMeta = "varn.Promise";
constexpr const char* AwaitsKey = "varn.awaits";
constexpr const char* AwaitCountKey = "varn.awaitCount";

struct PromiseUserdata
{
    std::shared_ptr<Promise> promise;
};

int Promise::luaGc(lua_State* L)
{
    auto* userdata = static_cast<PromiseUserdata*>(luaL_checkudata(L, 1, PromiseMeta));
    userdata->promise.~shared_ptr<Promise>();
    return 0;
}

int Promise::luaAwait(lua_State* L)
{
    auto* promise = Promise::check(L, 1);
    const int outcome = promise->prepareAwait(L);
    if (outcome == -1)
    {
        return lua_yield(L, 0);
    }

    return outcome;
}

int Promise::luaIsDone(lua_State* L)
{
    auto* promise = Promise::check(L, 1);
    lua_pushboolean(L, promise->state() != Promise::State::Pending);
    return 1;
}

Promise::Promise(Runtime& runtime)
    : runtime(runtime)
{
}

Promise::~Promise() = default;

void Promise::resolve(std::string resolvedValue)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (phase != State::Pending)
        {
            return;
        }

        phase = State::Resolved;
        customResolved = false;
        customPush = nullptr;
        value = std::move(resolvedValue);
    }

    resumeWaitersOnMainLoop();
}

void Promise::resolveCustom(std::function<void(lua_State* L)> pushResolved)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (phase != State::Pending)
        {
            return;
        }

        phase = State::Resolved;
        customResolved = true;
        value.clear();
        customPush = std::move(pushResolved);
    }

    resumeWaitersOnMainLoop();
}

void Promise::reject(std::string rejectedError)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (phase != State::Pending)
        {
            return;
        }

        phase = State::Rejected;
        customResolved = false;
        customPush = nullptr;
        error = std::move(rejectedError);
    }

    resumeWaitersOnMainLoop();
}

Promise::State Promise::state() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return phase;
}

void Promise::breakIfPending()
{
    // Reject a still-pending promise whose only resolver was discarded, mirroring `std::promise` broken-promise semantics so awaiters resume and their coroutine refs are released.
    if (runtime.stopped())
    {
        return;
    }

    reject("[Promise] The deferred resolver was discarded before the promise settled.");
}

// Pushes the table that names, for every coroutine, the await it is suspended in, or false once its task was cancelled, with weak keys so a finished coroutine leaves it.
void Promise::pushAwaits(lua_State* L)
{
    if (lua_getfield(L, LUA_REGISTRYINDEX, AwaitsKey) == LUA_TTABLE)
    {
        return;
    }

    lua_pop(L, 1);
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, AwaitsKey);
}

bool Promise::cancelled(lua_State* L)
{
    pushAwaits(L);
    lua_pushthread(L);
    lua_rawget(L, -2);
    const bool stopped = lua_isboolean(L, -1) && !lua_toboolean(L, -1);
    lua_pop(L, 2);
    return stopped;
}

// Names the await the running coroutine is about to be suspended in, so only a promise it still waits for resumes it.
lua_Integer Promise::beginAwait(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, AwaitCountKey);
    const lua_Integer token = lua_tointeger(L, -1) + 1;
    lua_pop(L, 1);
    lua_pushinteger(L, token);
    lua_setfield(L, LUA_REGISTRYINDEX, AwaitCountKey);

    pushAwaits(L);
    lua_pushthread(L);
    lua_pushinteger(L, token);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    return token;
}

// Answers whether the coroutine on top of the stack is still suspended in the await the token names, and takes it out of that await when it is.
bool Promise::claimResume(lua_State* L, lua_Integer token)
{
    lua_State* coroutine = lua_tothread(L, -1);
    if (coroutine == nullptr || lua_status(coroutine) != LUA_YIELD)
    {
        return false;
    }

    pushAwaits(L);
    lua_pushvalue(L, -2);
    lua_rawget(L, -2);
    const bool current = lua_isinteger(L, -1) && lua_tointeger(L, -1) == token;
    lua_pop(L, 1);

    if (current)
    {
        lua_pushvalue(L, -2);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }

    lua_pop(L, 1);
    return current;
}

// A cancelled task is never resumed by what it awaits, and it is closed, which runs its to-be-closed variables, at once when it is suspended and right after its next await when it is the one running.
void Promise::cancelTask(lua_State* L, int threadIndex)
{
    const int thread = lua_absindex(L, threadIndex);
    pushAwaits(L);
    lua_pushvalue(L, thread);
    lua_pushboolean(L, 0);
    lua_rawset(L, -3);
    lua_pop(L, 1);

    lua_State* task = lua_tothread(L, thread);
    if (task != nullptr && lua_status(task) == LUA_YIELD)
    {
        closeTask(L, task);
    }
}

// Closes a suspended task, and hands a failure one of its to-be-closed variables raised to the failure handler of the application.
void Promise::closeTask(lua_State* L, lua_State* task)
{
    if (lua_closethread(task, L) == LUA_OK)
    {
        return;
    }

    // The value a closing variable raised is the error of the failure, a table included, since the stack it was raised on is gone.
    lua_createtable(L, 0, 1);
    lua_xmove(task, L, 1);
    lua_setfield(L, -2, "error");
    std::ignore = AsyncModule::fail(L);
}

// Closes a cancelled task that reached an await once it has yielded, since a coroutine cannot close itself while it runs.
void Promise::closeAfterYield(lua_State* L)
{
    lua_pushthread(L);
    const int threadRef = luaL_ref(L, LUA_REGISTRYINDEX);
    Runtime* owner = &runtime;

    // clang-format off
    runtime.mainLoop().post([owner, threadRef]
    {
        if (owner->stopped())
        {
            return;
        }

        lua_State* mainState = owner->luaState();
        lua_rawgeti(mainState, LUA_REGISTRYINDEX, threadRef);
        lua_State* task = lua_tothread(mainState, -1);
        if (task != nullptr && lua_status(task) == LUA_YIELD)
        {
            closeTask(mainState, task);
        }

        lua_pop(mainState, 1);
        luaL_unref(mainState, LUA_REGISTRYINDEX, threadRef);
    });
    // clang-format on
}

int Promise::prepareAwait(lua_State* L)
{
    // A cancelled task stops at its await, even one that would answer at once, and is closed once it yielded there.
    if (cancelled(L))
    {
        closeAfterYield(L);
        return -1;
    }

    lua_pushthread(L);
    const int threadRef = luaL_ref(L, LUA_REGISTRYINDEX);

    std::function<void(lua_State*)> custom;
    bool settledResolvedString = false;
    bool settledResolvedCustom = false;
    std::string snapshotValue;
    std::string snapshotError;

    {
        std::lock_guard<std::mutex> lock(mutex);
        if (phase == State::Resolved)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, threadRef);
            if (customResolved)
            {
                custom = customPush;
                settledResolvedCustom = true;
            }
            else
            {
                snapshotValue = value;
                settledResolvedString = true;
            }
        }
        else if (phase == State::Rejected)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, threadRef);
            snapshotError = error;
        }
        else
        {
            waiting.push_back({threadRef, beginAwait(L)});
            return -1;
        }
    }

    if (settledResolvedString)
    {
        lua_pushlstring(L, snapshotValue.data(), snapshotValue.size());
        return 1;
    }

    if (settledResolvedCustom)
    {
        if (custom)
        {
            custom(L);
        }

        return 1;
    }

    lua_pushnil(L);
    lua_pushlstring(L, snapshotError.data(), snapshotError.size());
    return 2;
}

void Promise::runPendingResumes()
{
    // Skips the resume when the runtime is stopped and the Lua state is gone.
    if (runtime.stopped())
    {
        return;
    }

    lua_State* mainState = runtime.luaState();
    std::vector<Waiter> waiters;
    State phaseSnapshot;
    std::string valueSnapshot;
    std::string errorSnapshot;

    std::function<void(lua_State*)> customPushSnapshot;
    bool useCustom = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (waiting.empty())
        {
            return;
        }

        waiters.swap(waiting);
        phaseSnapshot = phase;

        if (phaseSnapshot == State::Resolved)
        {
            valueSnapshot = value;
            useCustom = customResolved;
            customPushSnapshot = customPush;
        }
        else
        {
            errorSnapshot = error;
        }
    }

    for (const Waiter& waiter : waiters)
    {
        const int ref = waiter.threadRef;
        lua_rawgeti(mainState, LUA_REGISTRYINDEX, ref);
        lua_State* coroutine = lua_tothread(mainState, -1);

        // A coroutine that left this await since, or whose task was cancelled, is not resumed with an answer it no longer waits for.
        const bool resumable = claimResume(mainState, waiter.token);
        lua_pop(mainState, 1);
        if (!resumable)
        {
            luaL_unref(mainState, LUA_REGISTRYINDEX, ref);
            continue;
        }

        int argCount = 0;

        if (phaseSnapshot == State::Resolved)
        {
            if (useCustom && customPushSnapshot)
            {
                customPushSnapshot(coroutine);
            }
            else
            {
                lua_pushlstring(coroutine, valueSnapshot.data(), valueSnapshot.size());
            }

            argCount = 1;
        }
        else
        {
            lua_pushnil(coroutine);
            lua_pushlstring(coroutine, errorSnapshot.data(), errorSnapshot.size());
            argCount = 2;
        }

#if defined(__EMSCRIPTEN__)
        lua_sethook(coroutine, nullptr, 0, 0);
#endif

        int nres = 0;
        int status = lua_resume(coroutine, mainState, argCount, &nres);
        if (status != LUA_OK && status != LUA_YIELD)
        {
            const char* message = lua_tostring(coroutine, -1);
            std::string detail = message ? message : "A task failed without a message.";
            log::Log::error("Promise", detail);
            if (nres > 0)
            {
                lua_pop(coroutine, nres);
            }
        }

        luaL_unref(mainState, LUA_REGISTRYINDEX, ref);
    }
}

void Promise::resumeWaitersOnMainLoop()
{
    auto self = shared_from_this();
    runtime.mainLoop().post([self]
                            { self->runPendingResumes(); });
}

Promise* Promise::check(lua_State* L, int index)
{
    auto* userdata = static_cast<PromiseUserdata*>(luaL_checkudata(L, index, PromiseMeta));
    return userdata->promise.get();
}

void Promise::push(lua_State* L, std::shared_ptr<Promise> promise)
{
    void* memory = lua_newuserdatauv(L, sizeof(PromiseUserdata), 0);
    new (memory) PromiseUserdata{std::move(promise)};

    luaL_getmetatable(L, PromiseMeta);
    lua_setmetatable(L, -2);
}

void Promise::installMetatable(lua_State* L)
{
    if (luaL_newmetatable(L, PromiseMeta) == 0)
    {
        lua_pop(L, 1);
        return;
    }

    lua_pushcfunction(L, &Promise::luaGc);
    lua_setfield(L, -2, "__gc");

    lua_newtable(L);

    lua_pushcfunction(L, &Promise::luaAwait);
    lua_setfield(L, -2, "await");

    lua_pushcfunction(L, &Promise::luaIsDone);
    lua_setfield(L, -2, "isDone");

    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
}

} // namespace varn::async
