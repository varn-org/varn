#include "varn/runtime/Runtime.h"
#include "varn/http/HttpTypes.h"
#include "varn/json/JsonSerializer.h"
#include "varn/log/Log.h"
#include "varn/lua/LuaEngine.h"

#include <lua.hpp>

#if defined(__EMSCRIPTEN__)
#include "varn/wasm/WasmAsyncHost.h"
#endif

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
    // host.on(name, fn) keeps the function in the registry so the host can reach it later from another thread
    static int hostOnTrampoline(lua_State* L)
    {
        auto* runtime = static_cast<Runtime*>(lua_touserdata(L, lua_upvalueindex(1)));

        const char* name = luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TFUNCTION);

        lua_pushvalue(L, 2);
        runtime->addHostEventHandler(name, luaL_ref(L, LUA_REGISTRYINDEX));
        return 0;
    }

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

    // the bridge exists from the start, so a script can subscribe with host.on even when the host registers no function
    ensureHostTable(engine->state());
    lua_pop(engine->state(), 1);
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
    beginChunk();
    return finishAfterUserChunk(engine->runFile(scriptPath));
}

int Runtime::runString(const std::string& source, const std::string& chunkName)
{
    beginChunk();
    return finishAfterUserChunk(engine->runString(source, chunkName));
}

// a runtime runs as many chunks as the host asks it to, so the outcome of the previous one decides nothing here
void Runtime::beginChunk()
{
    unhandledError = false;
    entryRequestedStop = false;
}

int Runtime::loadFile(const std::string& scriptPath)
{
    beginChunk();
    return finishAfterLoad(engine->runFile(scriptPath));
}

int Runtime::loadString(const std::string& source, const std::string& chunkName)
{
    beginChunk();
    return finishAfterLoad(engine->runString(source, chunkName));
}

// the chunk has run and whatever it left behind is now the host's to drive, so the loop is never entered here
int Runtime::finishAfterLoad(int loadRunExitCode)
{
    if (loadRunExitCode != 0)
    {
        return loadRunExitCode;
    }

    return unhandledError ? 1 : 0;
}

bool Runtime::poll()
{
    if (stopped())
    {
        return false;
    }

#if defined(__EMSCRIPTEN__)
    // the browser has no threads, so a pool job only ever runs when the pump drains it here
    bool progressed = false;
    do
    {
        progressed = false;
        if (pool.hasPostedJobs())
        {
            pool.drainPostedJobs();
            progressed = true;
        }

        if (ioPool().hasPostedJobs())
        {
            ioPool().drainPostedJobs();
            progressed = true;
        }

        if (loop.hasPendingJobs())
        {
            loop.drainPostedJobs();
            progressed = true;
        }
    } while (progressed);

    // a fetch resumes through a js callback and a timer matures on a later tick, so either one still expects more work
    const bool fetchPending = varn::wasm::WasmAsyncHost::fetchInflight().load(std::memory_order_acquire) > 0;
    return fetchPending || loop.hasPendingTimers() || loop.pending();
#else
    return loop.poll();
#endif
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

        return noServers && workLedger->depth() == 0;
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
    ensureHostTable(L);

    lua_pushlightuserdata(L, stored);
    lua_pushcclosure(L, &RuntimeHelpers::hostTrampoline, 1);
    lua_setfield(L, -2, name.c_str());
    lua_pop(L, 1);
}

// the host table is the whole native bridge: host.<name> reaches native, host.on receives from it
void Runtime::ensureHostTable(lua_State* L)
{
    lua_getglobal(L, "host");
    if (lua_istable(L, -1))
    {
        return;
    }

    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "host");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &RuntimeHelpers::hostOnTrampoline, 1);
    lua_setfield(L, -2, "on");
}

void Runtime::addHostEventHandler(const std::string& name, int luaRef)
{
    std::lock_guard<std::mutex> lock(handlersMutex);
    hostEventHandlers[name].push_back(luaRef);
}

void Runtime::emitHostEvent(const std::string& name, const std::string& jsonArgument)
{
    // the host calls this from its own thread, so delivery is posted and lua is only touched on the loop
    loop.post([this, name, jsonArgument]()
              {
        if (stopped())
        {
            return;
        }

        // a handler may register another one, so the list is copied before any of them runs
        std::vector<int> refs;
        {
            std::lock_guard<std::mutex> lock(handlersMutex);
            auto found = hostEventHandlers.find(name);
            if (found == hostEventHandlers.end())
            {
                return;
            }

            refs = found->second;
        }

        lua_State* L = engine->state();
        for (const int ref : refs)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!varn::json::JsonSerializer::deserialize(L, jsonArgument))
            {
                lua_pushnil(L);
            }

            if (lua_pcall(L, 1, 0, 0) != LUA_OK)
            {
                const char* message = lua_tostring(L, -1);
                log::Log::error("Runtime", message != nullptr ? message : "A host event handler failed.");
                lua_pop(L, 1);
            }
        } });
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
    loop.retain();
}

bool Runtime::releaseBackgroundDriver()
{
    return loop.release();
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
