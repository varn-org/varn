#pragma once

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/TaskPool.h"
#include "varn/runtime/WorkLedger.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

namespace varn::http
{
class HttpServer;
}

namespace varn::lua
{
class LuaEngine;
}

namespace varn::runtime
{

class Runtime
{
public:
    using HostFunction = std::function<std::string(const std::string&)>;

    explicit Runtime(std::vector<std::string> args, std::size_t scriptArgIndex = 1);
    ~Runtime();

    int runScript(const std::string& scriptPath);
    int runString(const std::string& source, const std::string& chunkName);

    int loadFile(const std::string& scriptPath);
    int loadString(const std::string& source, const std::string& chunkName);
    bool poll();
    bool runStringWithoutEventLoop(const std::string& source, const std::string& chunkName, std::string* errorMessage, void (*prePcallHook)(lua_State*, void*) = nullptr, void* prePcallUserdata = nullptr);

    EventLoop& mainLoop();
    TaskPool& taskPool();
    TaskPool& ioPool();
    lua_State* luaState();

    void registerHostFunction(const std::string& name, HostFunction fn);

    void addHostEventHandler(const std::string& name, int luaRef);
    void emitHostEvent(const std::string& name, const std::string& jsonArgument);

    void addServer(std::shared_ptr<varn::http::HttpServer> server);
    void stop();
    bool stopped() const { return stopFlag.load(std::memory_order_acquire); }

    void onAsyncComplete(bool ok, bool stopLoopOnSuccess, const std::string& error);

    void retainBackgroundDriver();
    bool releaseBackgroundDriver();

    const std::vector<std::string>& args() const;
    std::size_t scriptArgIndex() const { return scriptArgPosition; }

private:
    std::vector<std::string> arguments;
    std::size_t scriptArgPosition;
    std::shared_ptr<WorkLedger> workLedger;
    EventLoop loop;
    TaskPool pool;
    mutable std::mutex ioWorkersMutex;
    std::unique_ptr<TaskPool> ioWorkers;
    std::unique_ptr<varn::lua::LuaEngine> engine;
    std::vector<std::unique_ptr<HostFunction>> hostFunctions;
    mutable std::mutex handlersMutex;
    std::unordered_map<std::string, std::vector<int>> hostEventHandlers;
    mutable std::mutex serversMutex;
    std::vector<std::shared_ptr<varn::http::HttpServer>> servers;
    std::atomic<bool> stopFlag{false};
    bool unhandledError = false;
    bool entryRequestedStop = false;

    void ensureHostTable(lua_State* L);
    void beginChunk();
    int finishAfterUserChunk(int loadRunExitCode);
    int finishAfterLoad(int loadRunExitCode);
};

} // namespace varn::runtime
