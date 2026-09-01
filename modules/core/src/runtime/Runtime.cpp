#include "varn/runtime/Runtime.h"
#include "varn/http/HttpTypes.h"
#include "varn/json/JsonSerializer.h"
#include "varn/log/Log.h"
#include "varn/lua/LuaEngine.h"

#include <lua.hpp>

namespace varn::runtime
{

using varn::lua::LuaEngine;

namespace
{
// size a dedicated pool for blocking i/o (http client, filesystem) so it never starves the cpu task pool
constexpr std::size_t kIoThreads = 32;

class RuntimeHelpers
{
public:
    // bridges a Lua call to a host function, marshalling the single argument and the result through json
    static int hostTrampoline(lua_State* L)
    {
        auto* fn = static_cast<Runtime::HostFunction*>(lua_touserdata(L, lua_upvalueindex(1)));

        const std::string argument = lua_gettop(L) >= 1 ? varn::json::JsonSerializer::serialize(L, 1) : std::string("null");
        const std::string result = (*fn)(argument);

        if (!varn::json::JsonSerializer::deserialize(L, result))
        {
            lua_pushnil(L);
        }

        return 1;
    }
};
} // namespace

Runtime::Runtime(std::vector<std::string> args, std::size_t scriptArgIndex)
    : arguments(std::move(args))
    , scriptArgPosition(scriptArgIndex)
    , workLedger(std::make_shared<WorkLedger>())
    , loop(workLedger)
    , pool(std::thread::hardware_concurrency(), workLedger)
    , engine(std::make_unique<LuaEngine>(*this))
{
    // install the notify hook before any worker spins up so none can observe it unset
    workLedger->setNotify([this]
                          { loop.wake(); });
    pool.start();
}

Runtime::~Runtime()
{
    stop();

#if !defined(__EMSCRIPTEN__)
    // a script that fails before the loop is entered leaves its socket watchers armed, and those handlers hold lua registry refs, so they are released here while the engine still owns the state rather than after it is closed
    loop.shutdownIo();
#endif
}

int Runtime::runScript(const std::string& scriptPath)
{
    return finishAfterUserChunk(engine->runFile(scriptPath));
}

int Runtime::runString(const std::string& source, const std::string& chunkName)
{
    return finishAfterUserChunk(engine->runString(source, chunkName));
}

bool Runtime::runStringWithoutEventLoop(const std::string& source, const std::string& chunkName, std::string* errorMessage, void (*prePcallHook)(lua_State*, void*), void* prePcallUserdata)
{
    return engine->runStringWithoutEventLoop(source, chunkName, errorMessage, prePcallHook, prePcallUserdata);
}

int Runtime::finishAfterUserChunk(int loadRunExitCode)
{
    if (loadRunExitCode != 0)
    {
        return loadRunExitCode;
    }

    // handle an async entry that already failed or completed synchronously before the loop starts
    if (unhandledError)
    {
        return 1;
    }

    if (entryRequestedStop)
    {
        return 0;
    }

    // clang-format off
    loop.setIdleExitPredicate([this]
    {
        bool noServers;
        {
            std::lock_guard<std::mutex> lock(serversMutex);
            noServers = servers.empty();
        }

        return noServers && backgroundDrivers.load(std::memory_order_acquire) == 0 && workLedger->depth() == 0;
    });
    // clang-format on
    loop.run();
    loop.setIdleExitPredicate({});
    return unhandledError ? 1 : 0;
}

void Runtime::onAsyncComplete(bool ok, bool stopLoopOnSuccess, const std::string& error)
{
    if (!ok)
    {
        unhandledError = true;
        log::Log::error("Runtime", error.empty() ? "A task failed without a message." : error);
        loop.stop();
        return;
    }

    if (stopLoopOnSuccess)
    {
        entryRequestedStop = true;
        loop.stop();
    }
}

EventLoop& Runtime::mainLoop()
{
    return loop;
}

TaskPool& Runtime::taskPool()
{
    return pool;
}

TaskPool& Runtime::ioPool()
{
    // the pool is created on first use but stop() can reach it from another thread, so publication is guarded
    std::lock_guard<std::mutex> lock(ioWorkersMutex);
    if (!ioWorkers)
    {
        ioWorkers = std::make_unique<TaskPool>(kIoThreads, workLedger);
        ioWorkers->start();
    }

    return *ioWorkers;
}

lua_State* Runtime::luaState()
{
    return engine->state();
}

void Runtime::registerHostFunction(const std::string& name, HostFunction fn)
{
    hostFunctions.push_back(std::make_unique<HostFunction>(std::move(fn)));
    HostFunction* stored = hostFunctions.back().get();

    lua_State* L = engine->state();

    // create or reuse the global host table, then bind name to a closure carrying the stored function pointer
    lua_getglobal(L, "host");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "host");
    }

    lua_pushlightuserdata(L, stored);
    lua_pushcclosure(L, &RuntimeHelpers::hostTrampoline, 1);
    lua_setfield(L, -2, name.c_str());
    lua_pop(L, 1);
}

void Runtime::addServer(std::shared_ptr<varn::http::HttpServer> server)
{
    {
        std::lock_guard<std::mutex> lock(serversMutex);
        servers.push_back(std::move(server));
    }

    loop.wake();
}

void Runtime::retainBackgroundDriver()
{
    backgroundDrivers.fetch_add(1, std::memory_order_relaxed);
}

void Runtime::releaseBackgroundDriver()
{
    backgroundDrivers.fetch_sub(1, std::memory_order_acq_rel);
    loop.wake();
}

void Runtime::stop()
{
    bool expected = false;
    if (!stopFlag.compare_exchange_strong(expected, true))
    {
        return;
    }

    // snapshot and clear under the lock so a concurrent addServer on the loop thread never races the teardown
    std::vector<std::shared_ptr<varn::http::HttpServer>> pending;
    {
        std::lock_guard<std::mutex> lock(serversMutex);
        pending.swap(servers);
    }

    for (auto& server : pending)
    {
        if (server)
        {
            server->stop();
        }
    }

    pool.stop();

    // read the lazily created io pool under its mutex so a concurrent first use on the loop thread cannot be missed
    TaskPool* io = nullptr;
    {
        std::lock_guard<std::mutex> lock(ioWorkersMutex);
        io = ioWorkers.get();
    }

    if (io)
    {
        io->stop();
    }

    loop.stop();
    // drop still-queued jobs to release their captured state such as lua registry refs while the lua_State is still alive
    loop.clearPendingJobs();
}

const std::vector<std::string>& Runtime::args() const
{
    return arguments;
}

} // namespace varn::runtime
