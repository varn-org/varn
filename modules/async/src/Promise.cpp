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

// Lua interns every string up to this length, so only a longer one is worth keeping for later awaiters.
constexpr std::size_t kSharedStringBytes = 40;

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

// The failure a task kept is released only while the loop still drives the Lua state, since `lua_close` reclaims the registry anyway.
// The shared string of the value is released on the loop, since the last owner of a promise may be a pool thread.
Promise::~Promise()
{
    if (failureRef != LUA_NOREF && !runtime.stopped())
    {
        luaL_unref(runtime.luaState(), LUA_REGISTRYINDEX, failureRef);
    }

    if (valueRef == LUA_NOREF || runtime.stopped())
    {
        return;
    }

    Runtime* owner = &runtime;
    const int ref = valueRef;
    // clang-format off
    runtime.mainLoop().post([owner, ref]
    {
        if (!owner->stopped())
        {
            luaL_unref(owner->luaState(), LUA_REGISTRYINDEX, ref);
        }
    });
    // clang-format on
}

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
    std::ignore = rejectWith(std::move(rejectedError), LUA_NOREF, false);
}

// Rejects with a failure the failure handler already received, so nothing reports it again when no one observes the rejection.
void Promise::rejectReported(std::string rejectedError)
{
    std::ignore = rejectWith(std::move(rejectedError), LUA_NOREF, true);
}

// Rejects with the failure at the top of the stack, the table `AsyncModule::capture` builds or a bare error, and pops it.
// Awaiters receive the text of its error, and its error as raised, its traceback and its frames reach the failure handler when no one observes the rejection.
void Promise::rejectFailure(lua_State* L, const char* fallback)
{
    AsyncModule::wrapFailure(L);

    lua_getfield(L, -1, "error");
    const char* message = lua_tostring(L, -1);
    std::string text = message != nullptr ? message : fallback;
    lua_pop(L, 1);

    const int failure = luaL_ref(L, LUA_REGISTRYINDEX);
    if (!rejectWith(std::move(text), failure, false))
    {
        luaL_unref(L, LUA_REGISTRYINDEX, failure);
    }
}

bool Promise::rejectWith(std::string rejectedError, int failure, bool alreadyReported)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (phase != State::Pending)
        {
            return false;
        }

        phase = State::Rejected;
        customResolved = false;
        customPush = nullptr;
        error = std::move(rejectedError);
        failureRef = failure;
        reported = alreadyReported;
    }

    resumeWaitersOnMainLoop();
    return true;
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

    // Nothing failed, so a broken promise no one observes is never reported.
    rejectReported("[Promise] The deferred resolver was discarded before the promise settled.");
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
    // Every await observes the promise, so its rejection is never reported as unhandled.
    {
        std::lock_guard<std::mutex> lock(mutex);
        observed = true;
    }

    // A cancelled task stops at its await, even one that would answer at once, and is closed once it yielded there.
    if (cancelled(L))
    {
        closeAfterYield(L);
        return -1;
    }

    // The outcome never changes once settled, so it is read in place after the lock and never copied per awaiter.
    State settled;
    {
        std::lock_guard<std::mutex> lock(mutex);
        settled = phase;
        if (settled == State::Pending)
        {
            lua_pushthread(L);
            waiting.push_back({luaL_ref(L, LUA_REGISTRYINDEX), beginAwait(L)});
            return -1;
        }
    }

    if (settled == State::Resolved)
    {
        pushResolved(L);
        return 1;
    }

    lua_pushnil(L);
    lua_pushlstring(L, error.data(), error.size());
    return 2;
}

// Pushes the resolved value, which every awaiter shares: a long string becomes one Lua string the first time and a custom value is pushed by its function.
void Promise::pushResolved(lua_State* L)
{
    if (customResolved)
    {
        if (customPush)
        {
            customPush(L);
        }

        return;
    }

    if (valueRef != LUA_NOREF)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, valueRef);
        return;
    }

    lua_pushlstring(L, value.data(), value.size());

    // Lua interns a short string by itself, so only a long one is kept for the awaiters to come, and the native copy goes once Lua holds it.
    if (value.size() > kSharedStringBytes)
    {
        lua_pushvalue(L, -1);
        valueRef = luaL_ref(L, LUA_REGISTRYINDEX);
        value = std::string();
    }
}

void Promise::runPendingResumes()
{
    // Skips the resume when the runtime is stopped and the Lua state is gone.
    if (runtime.stopped())
    {
        return;
    }

    lua_State* mainState = runtime.luaState();
    if (claimUnobservedRejection())
    {
        reportRejection(mainState);
        return;
    }

    std::vector<Waiter> waiters;
    State settled;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (waiting.empty())
        {
            return;
        }

        waiters.swap(waiting);
        settled = phase;
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

        int argCount = 1;
        if (settled == State::Resolved)
        {
            pushResolved(coroutine);
        }
        else
        {
            lua_pushnil(coroutine);
            lua_pushlstring(coroutine, error.data(), error.size());
            argCount = 2;
        }

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

// Answers whether the promise rejected with no one observing it and no report made yet, and claims the report when it did.
bool Promise::claimUnobservedRejection()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (phase != State::Rejected || observed || reported)
    {
        return false;
    }

    reported = true;
    return true;
}

// Hands an unobserved rejection to the failure handler as a rejection, with the error as raised, the traceback and the frames of a failed task, or with the message alone when native code rejected it.
void Promise::reportRejection(lua_State* L)
{
    if (failureRef != LUA_NOREF)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, failureRef);
    }
    else
    {
        lua_createtable(L, 0, 2);
        std::string message;
        {
            std::lock_guard<std::mutex> lock(mutex);
            message = error;
        }

        lua_pushlstring(L, message.data(), message.size());
        lua_setfield(L, -2, "error");
    }

    lua_pushliteral(L, "rejection");
    lua_setfield(L, -2, "kind");
    std::ignore = AsyncModule::fail(L);
}

// Posts the resume of the coroutines that wait on the promise, and posts nothing when none waits and no unobserved rejection is left to report, since an await that comes later reads the settled promise in place.
void Promise::resumeWaitersOnMainLoop()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        const bool unobservedRejection = phase == State::Rejected && !observed && !reported;
        if (waiting.empty() && !unobservedRejection)
        {
            return;
        }
    }

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

void Promise::pushMethods(lua_State* L)
{
    installMetatable(L);
    luaL_getmetatable(L, PromiseMeta);
    lua_getfield(L, -1, "__index");
    lua_remove(L, -2);
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
