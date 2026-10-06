#pragma once

#include "varn/runtime/Scheduler.h"

#include <atomic>
#include <cstddef>
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
    using ModuleOpener = int (*)(lua_State*);

    Runtime(Scheduler& scheduler, std::vector<std::string> args, std::size_t scriptArgIndex = 1);
    ~Runtime();

    int runScript(const std::string& scriptPath);
    int runString(const std::string& source, const std::string& chunkName);

    int loadFile(const std::string& scriptPath);
    int loadString(const std::string& source, const std::string& chunkName);
    bool runStringWithoutEventLoop(const std::string& source, const std::string& chunkName, std::string& errorMessage);

    Scheduler& scheduler();
    lua_State* luaState();

    void registerHostFunction(const std::string& name, HostFunction fn);
    [[nodiscard]] bool addModule(const std::string& name, ModuleOpener opener);
    [[nodiscard]] bool addSymbol(const std::string& name, void* address);

    void addHostEventHandler(const std::string& name, int luaRef);
    void emitHostEvent(const std::string& name, const std::string& jsonArgument);

    void addServer(std::shared_ptr<varn::http::HttpServer> server);
    void removeServer(const varn::http::HttpServer* server);
    void stop();
    bool stopped() const;

    void onAsyncComplete(bool ok, bool stopLoopOnSuccess, const std::string& error);

    const std::vector<std::string>& args() const;
    std::size_t scriptArgIndex() const { return scriptArgPosition; }

private:
    std::vector<std::string> arguments;
    std::size_t scriptArgPosition;
    Scheduler& workScheduler;
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
#if defined(VARN_EVENT_LOOP_DRIVER_PUMP)
    static int refuseRun();
#else
    int finishAfterUserChunk(int loadRunExitCode);
#endif
    int finishAfterLoad(int loadRunExitCode);
};

} // namespace varn::runtime
