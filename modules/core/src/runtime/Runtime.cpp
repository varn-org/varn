#include "varn/runtime/Runtime.h"
#include "varn/http/HttpTypes.h"
#include "varn/json/JsonSerializer.h"
#include "varn/log/Log.h"
#include "varn/lua/LuaEngine.h"

#include <lua.hpp>

#include <chrono>
#include <thread>

#if defined(__EMSCRIPTEN__)
#include "varn/wasm/WasmAsyncHost.h"
#endif

namespace varn::runtime
{

using varn::lua::LuaEngine;

namespace
{
// The threads of the I/O pool mostly wait on files, processes and transfers, so it may run many more of them than there are cores.
constexpr std::size_t kIoThreads = 512;

constexpr std::size_t kTaskThreadsWithoutCoreCount = 4;

constexpr std::chrono::milliseconds kWorkerKeepAlive{10000};

// The table of the symbols a host added, which the ffi module reads before it asks the dynamic linker.
constexpr const char* kSymbolsKey = "varn.ffi.symbols";

class RuntimeHelpers
{
public:
    // The standard answers zero when it cannot count the cores.
    static std::size_t coreCount()
    {
        const unsigned cores = std::thread::hardware_concurrency();
        return cores != 0 ? cores : kTaskThreadsWithoutCoreCount;
    }

    // Keeps the subscribed function in the registry so the host can reach it later from another thread.
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

// A pool the options leave at zero takes its default limit, a thread per core for the tasks and a high count for the blocking I/O.
Runtime::Runtime(std::vector<std::string> args, std::size_t scriptArgIndex, Options options)
    : arguments(std::move(args))
    , scriptArgPosition(scriptArgIndex)
    , workLedger(std::make_shared<WorkLedger>())
    , loop(workLedger)
    , taskWorkers(options.taskThreads != 0 ? options.taskThreads : RuntimeHelpers::coreCount(), kWorkerKeepAlive, workLedger)
    , ioWorkers(options.ioThreads != 0 ? options.ioThreads : kIoThreads, kWorkerKeepAlive, workLedger)
    , engine(std::make_unique<LuaEngine>(*this))
{
    // No pool starts a worker before its first job, which nothing posts before the hook is installed here.
    // The ledger notifies only when its last entry leaves, which is the one change that can let an idle loop exit.
    workLedger->setNotify([this]
                          { loop.wake(); });

    // The bridge exists from the start, so a script can subscribe with `host.on` even when the host registers no function.
    ensureHostTable(engine->state());
    lua_pop(engine->state(), 1);
}

Runtime::~Runtime()
{
    stop();

#if !defined(__EMSCRIPTEN__)
    // A script that fails before the loop is entered leaves its socket watchers armed, and those handlers hold Lua registry refs, so they are released here while the engine still owns the state rather than after it is closed.
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

// A runtime runs as many chunks as the host asks it to, so the outcome of the previous one decides nothing here.
void Runtime::beginChunk()
{
    unhandledError = false;
    entryRequestedStop = false;

    // The chunk thread is the loop thread from the first line of the chunk, so a socket the chunk watches reaches the loop in order with a close from another thread.
    // A stop sets its flag before it stops the loop, so a stop that the check misses lands after the entry opened and still ends it.
    loop.enter();
    if (stopped())
    {
        loop.stop();
    }
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

// The chunk has run and whatever it left behind is now the host's to drive, so the loop is never entered here.
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
    // The browser has no threads, so a pool job only ever runs when the pump drains it here.
    bool progressed = false;
    do
    {
        progressed = drainPools(std::chrono::steady_clock::time_point::max());
        if (loop.hasPendingJobs())
        {
            loop.drainPostedJobs();
            progressed = true;
        }
    } while (progressed);

    const bool more = loop.poll();
    // A fetch resumes through a JavaScript callback rather than through the queue, so it still expects a later tick, and so does a job the loop just posted to a pool.
    const bool fetchPending = varn::wasm::WasmAsyncHost::fetchInflight().load(std::memory_order_acquire) > 0;
    return more || fetchPending || poolsHaveJobs();
#else
    return loop.poll();
#endif
}

// Advances the runtime without blocking for about as long as the budget allows, and answers whether anything ran, whether anything can still make progress and how long the host can sleep.
EventLoop::PollResult Runtime::poll(std::chrono::nanoseconds budget)
{
    if (stopped())
    {
        return {};
    }

#if defined(__EMSCRIPTEN__)
    // The browser has no threads, so the pools are drained before every poll of the loop and the loop is polled again while either one makes progress.
    const auto started = std::chrono::steady_clock::now();
    EventLoop::PollResult result;
    for (;;)
    {
        const bool drained = drainPools(started + budget);
        const auto spent = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started);
        const EventLoop::PollResult step = loop.poll(budget > spent ? budget - spent : std::chrono::nanoseconds::zero());
        result.ran = result.ran || drained || step.ran;
        result.pending = step.pending;
        result.idleMilliseconds = step.idleMilliseconds;
        if ((!drained && !step.ran) || std::chrono::steady_clock::now() - started >= budget)
        {
            break;
        }
    }

    // A job a pass posted to a pool runs in the next poll, so the host polls again at once.
    if (poolsHaveJobs())
    {
        result.pending = true;
        result.idleMilliseconds = 0;
    }

    // A fetch resumes through a JavaScript callback rather than through the queue, so it still expects a later poll.
    result.pending = result.pending || varn::wasm::WasmAsyncHost::fetchInflight().load(std::memory_order_acquire) > 0;
    return result;
#else
    return loop.poll(budget);
#endif
}

#if defined(__EMSCRIPTEN__)
// Runs pool jobs until none is left or the deadline passed, answering whether any ran.
bool Runtime::drainPools(std::chrono::steady_clock::time_point deadline)
{
    const bool tasks = taskWorkers.drainPostedJobs(deadline);
    const bool io = ioWorkers.drainPostedJobs(deadline);
    return tasks || io;
}

bool Runtime::poolsHaveJobs()
{
    return taskWorkers.hasPostedJobs() || ioWorkers.hasPostedJobs();
}
#endif

bool Runtime::runStringWithoutEventLoop(const std::string& source, const std::string& chunkName, std::string* errorMessage)
{
    return engine->runStringWithoutEventLoop(source, chunkName, errorMessage);
}

int Runtime::finishAfterUserChunk(int loadRunExitCode)
{
    if (loadRunExitCode != 0)
    {
        return loadRunExitCode;
    }

    // Handle an async entry that already failed or completed synchronously before the loop starts.
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
        log::Log::error(error.empty() ? "A task failed without a message." : error);
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
    return taskWorkers;
}

TaskPool& Runtime::ioPool()
{
    return ioWorkers;
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

// Adds a native module that Lua loads with `require(name)`, and refuses a name that a module of the runtime or an earlier call already answers.
bool Runtime::addModule(const std::string& name, ModuleOpener opener)
{
    if (name.empty() || opener == nullptr)
    {
        return false;
    }

    lua_State* L = engine->state();
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    const bool loaded = lua_getfield(L, -1, name.c_str()) != LUA_TNIL;
    lua_pop(L, 2);

    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    if (loaded || lua_getfield(L, -1, name.c_str()) != LUA_TNIL)
    {
        lua_pop(L, loaded ? 1 : 2);
        return false;
    }

    lua_pop(L, 1);
    lua_pushcfunction(L, opener);
    lua_setfield(L, -2, name.c_str());
    lua_pop(L, 1);
    return true;
}

// Adds a native function or value that `ffi.C` finds by name before it asks the dynamic linker, for a symbol linked statically where no export reaches it, and refuses a name already added with another address.
bool Runtime::addSymbol(const std::string& name, void* address)
{
    if (name.empty() || address == nullptr)
    {
        return false;
    }

    lua_State* L = engine->state();
    luaL_getsubtable(L, LUA_REGISTRYINDEX, kSymbolsKey);
    const bool taken = lua_getfield(L, -1, name.c_str()) == LUA_TLIGHTUSERDATA && lua_touserdata(L, -1) != address;
    lua_pop(L, 1);

    if (taken)
    {
        lua_pop(L, 1);
        return false;
    }

    lua_pushlightuserdata(L, address);
    lua_setfield(L, -2, name.c_str());
    lua_pop(L, 1);
    return true;
}

// The host table is the whole native bridge: `host.<name>` reaches native, `host.on` receives from it.
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
    // The host calls this from its own thread, so delivery is posted and Lua is only touched on the loop.
    loop.post([this, name, jsonArgument]()
              {
        if (stopped())
        {
            return;
        }

        // A handler may register another one, so the list is copied before any of them runs.
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
                log::Log::error(message != nullptr ? message : "A host event handler failed.");
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

// Lets go of a server that closed, so the loop can end once nothing else keeps it running.
void Runtime::removeServer(const varn::http::HttpServer* server)
{
    // The server is released after the lock, since its destructor releases the Lua references of its handlers.
    std::shared_ptr<varn::http::HttpServer> released;
    {
        std::lock_guard<std::mutex> lock(serversMutex);
        for (auto it = servers.begin(); it != servers.end(); ++it)
        {
            if (it->get() == server)
            {
                released = std::move(*it);
                servers.erase(it);
                break;
            }
        }
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

    // Snapshot and clear under the lock so a concurrent `addServer` on the loop thread never races the teardown.
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

    taskWorkers.stop();
    ioWorkers.stop();

    loop.stop();
    // Drop still-queued jobs to release their captured state such as Lua registry refs while the `lua_State` is still alive.
    loop.clearPendingJobs();
}

const std::vector<std::string>& Runtime::args() const
{
    return arguments;
}

} // namespace varn::runtime
