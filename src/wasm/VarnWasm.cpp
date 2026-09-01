#include "varn/runtime/Runtime.h"
#include "varn/wasm/WasmAsyncHost.h"

#include <lua.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#endif

namespace varn::wasm
{

struct RunResult
{
    bool ok = false;
    std::string output;
    std::string error;
};

class WasmHost
{
public:
    WasmHost() = delete;

    static RunResult runChunk(const std::string& source);

private:
    struct PrintSinkScope
    {
        explicit PrintSinkScope(std::string* sink) { printSink = sink; }
        PrintSinkScope(const PrintSinkScope&) = delete;
        PrintSinkScope& operator=(const PrintSinkScope&) = delete;
        ~PrintSinkScope() { printSink = nullptr; }
    };

    static int luaPrintCapture(lua_State* L);

#if defined(__EMSCRIPTEN__)
    static void lineHook(lua_State* L, lua_Debug* ar);
    static void attachLineHook(lua_State* L, void* userdata);
    static void pumpDeferredWork(varn::runtime::Runtime& rt);
#endif

    static std::unique_ptr<varn::runtime::Runtime>& runtime();

    inline static std::string* printSink = nullptr;
#if defined(__EMSCRIPTEN__)
    inline static int hookTick = 0;
#endif
};

int WasmHost::luaPrintCapture(lua_State* L)
{
    if (!printSink)
    {
        return 0;
    }

    const int n = lua_gettop(L);
    std::string line;

    for (int i = 1; i <= n; ++i)
    {
        if (i > 1)
        {
            line += '\t';
        }

        size_t len = 0;
        const char* chunk = luaL_tolstring(L, i, &len);
        line.append(chunk, len);
        lua_pop(L, 1);
    }

    line += '\n';
    printSink->append(line);
    return 0;
}

#if defined(__EMSCRIPTEN__)
void WasmHost::lineHook(lua_State* /*L*/, lua_Debug* /*ar*/)
{
    ++hookTick;
    if ((hookTick % 32) == 0)
    {
        emscripten_sleep(0);
    }
}

void WasmHost::attachLineHook(lua_State* L, void*)
{
    hookTick = 0;
    lua_sethook(L, &WasmHost::lineHook, LUA_MASKCOUNT, 500);
}

void WasmHost::pumpDeferredWork(varn::runtime::Runtime& rt)
{
    const double deadlineMs = emscripten_get_now() + 120000.0;
    while (emscripten_get_now() < deadlineMs)
    {
        bool progressed;
        do
        {
            progressed = false;
            if (rt.taskPool().hasPostedJobs())
            {
                rt.taskPool().drainPostedJobs();
                progressed = true;
            }

            if (rt.ioPool().hasPostedJobs())
            {
                rt.ioPool().drainPostedJobs();
                progressed = true;
            }

            if (rt.mainLoop().hasPendingJobs())
            {
                rt.mainLoop().drainPostedJobs();
                progressed = true;
            }
        } while (progressed);

        // a pending fetch resumes via a js callback and an undue timer matures on the next pump, so either condition still expects more work even when no job ran this iteration
        const bool fetchPending = varn::wasm::WasmAsyncHost::fetchInflight().load(std::memory_order_acquire) > 0;
        const bool timerPending = rt.mainLoop().hasPendingTimers();
        if (fetchPending || timerPending)
        {
            emscripten_sleep(0);
            continue;
        }

        break;
    }
}
#endif

std::unique_ptr<varn::runtime::Runtime>& WasmHost::runtime()
{
    static std::unique_ptr<varn::runtime::Runtime> instance;
    return instance;
}

RunResult WasmHost::runChunk(const std::string& source)
{
    RunResult result;

    auto& rtPtr = runtime();
    if (!rtPtr)
    {
        rtPtr = std::make_unique<varn::runtime::Runtime>(std::vector<std::string>{});
        lua_State* Lsetup = rtPtr->luaState();
        lua_pushcfunction(Lsetup, &WasmHost::luaPrintCapture);
        lua_setglobal(Lsetup, "print");
    }

    varn::runtime::Runtime& rt = *rtPtr;
    lua_State* L = rt.luaState();

    std::string collected;
    std::string err;
    bool ok = false;

    {
        PrintSinkScope sinkScope(&collected);
#if defined(__EMSCRIPTEN__)
        ok = rt.runStringWithoutEventLoop(source, "=wasm", &err, &WasmHost::attachLineHook, nullptr);
        lua_sethook(L, nullptr, 0, 0);

        pumpDeferredWork(rt);
#else
        ok = rt.runStringWithoutEventLoop(source, "=wasm", &err);
#endif
    }

    result.output = std::move(collected);
    result.ok = ok;
    if (!ok)
    {
        result.error = std::move(err);
    }

    return result;
}

} // namespace varn::wasm

#if defined(__EMSCRIPTEN__)
EMSCRIPTEN_BINDINGS(varn_wasm)
{
    emscripten::value_object<varn::wasm::RunResult>("RunResult")
        .field("ok", &varn::wasm::RunResult::ok)
        .field("output", &varn::wasm::RunResult::output)
        .field("error", &varn::wasm::RunResult::error);

    emscripten::function("varnRunChunk", &varn::wasm::WasmHost::runChunk);
}
#endif
