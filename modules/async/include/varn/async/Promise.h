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
    void breakIfPending();

    State state() const;

    void resumeWaitersOnMainLoop();

    int prepareAwait(lua_State* L);

    static void installMetatable(lua_State* L);
    static void push(lua_State* L, std::shared_ptr<Promise> promise);
    static Promise* check(lua_State* L, int index);

    void runPendingResumes();

private:
    static int luaGc(lua_State* L);
    static int luaAwait(lua_State* L);
    static int luaIsDone(lua_State* L);

    mutable std::mutex mutex;
    varn::runtime::Runtime& runtime;
    State phase = State::Pending;
    std::string value;
    std::string error;
    bool customResolved = false;
    std::function<void(lua_State* L)> customPush;
    std::vector<int> waitingRefs;
};

} // namespace varn::async
