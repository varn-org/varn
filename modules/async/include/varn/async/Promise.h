#pragma once

#include <functional>
#include <lua.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace varn::runtime
{
class Runtime;
}

namespace varn::async
{

class Promise : public std::enable_shared_from_this<Promise>
{
public:
    enum class State
    {
        Pending,
        Resolved,
        Rejected
    };

    explicit Promise(varn::runtime::Runtime& runtime);
    ~Promise();

    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;

    void resolve(std::string value = {});
    void resolveCustom(std::function<void(lua_State* L)> pushResolved);
    void reject(std::string error);
    void rejectReported(std::string error);
    void rejectFailure(lua_State* L, const char* fallback);
    void breakIfPending();

    State state() const;

    void resumeWaitersOnMainLoop();

    int prepareAwait(lua_State* L);

    static void installMetatable(lua_State* L);
    static void pushMethods(lua_State* L);
    static void push(lua_State* L, std::shared_ptr<Promise> promise);
    static Promise* check(lua_State* L, int index);
    static void cancelTask(lua_State* L, int threadIndex);

    void runPendingResumes();

private:
    struct Waiter
    {
        int threadRef;
        lua_Integer token;
    };

    static void pushAwaits(lua_State* L);
    static bool cancelled(lua_State* L);
    static lua_Integer beginAwait(lua_State* L);
    static bool claimResume(lua_State* L, lua_Integer token);
    static void closeTask(lua_State* L, lua_State* task);
    void closeAfterYield(lua_State* L);
    void pushResolved(lua_State* L);
    bool rejectWith(std::string rejectedError, int failure, bool alreadyReported);
    bool claimUnobservedRejection();
    void reportRejection(lua_State* L);
    static int luaGc(lua_State* L);
    static int luaAwait(lua_State* L);
    static int luaIsDone(lua_State* L);

    mutable std::mutex mutex;
    varn::runtime::Runtime& runtime;
    State phase = State::Pending;
    std::string value;
    int valueRef = LUA_NOREF;
    std::string error;
    bool customResolved = false;
    std::function<void(lua_State* L)> customPush;
    std::vector<Waiter> waiting;
    bool observed = false;
    bool reported = false;
    int failureRef = LUA_NOREF;
};

} // namespace varn::async
