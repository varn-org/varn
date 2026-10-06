#include "varn/runtime/Runtime.h"

#include <emscripten/bind.h>
#include <emscripten/em_js.h>
#include <lua.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

// Wakes the pump of `varnRunChunk`, which waits in JavaScript for its next timer or for the work a callback of the page posts, such as the end of a fetch.
EM_JS(void, varn_wasm_wake, (), { varnWakeUp(); });

namespace varn::wasm
{

struct RunResult
{
    bool ok = false;
    std::string output;
    std::string error;
};

struct PollResult
{
    bool ran = false;
    bool pending = false;
    double idleMilliseconds = -1;
    std::string output;
};

class WasmHost
{
public:
    WasmHost() = delete;

    static RunResult loadChunk(const std::string& source);
    static bool poll();
    static PollResult pollBudget(double budgetMilliseconds);
    static void registerHostFunction(const std::string& name, emscripten::val callback);
    static bool emit(const std::string& name, const std::string& jsonArgument);

private:
    static varn::runtime::Runtime& ensureRuntime();

    struct PrintSinkScope
    {
        explicit PrintSinkScope(std::string* sink) { printSink = sink; }
        PrintSinkScope(const PrintSinkScope&) = delete;
        PrintSinkScope& operator=(const PrintSinkScope&) = delete;
        ~PrintSinkScope() { printSink = nullptr; }
    };

    static int luaPrintCapture(lua_State* L);

    static std::unique_ptr<varn::runtime::Runtime>& runtime();

    inline static std::string* printSink = nullptr;
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

std::unique_ptr<varn::runtime::Runtime>& WasmHost::runtime()
{
    static std::unique_ptr<varn::runtime::Runtime> instance;
    return instance;
}

varn::runtime::Runtime& WasmHost::ensureRuntime()
{
    auto& rtPtr = runtime();
    if (!rtPtr)
    {
        rtPtr = std::make_unique<varn::runtime::Runtime>(std::vector<std::string>{});
        rtPtr->mainLoop().setWakeHandler(&varn_wasm_wake);
        lua_State* Lsetup = rtPtr->luaState();
        lua_pushcfunction(Lsetup, &WasmHost::luaPrintCapture);
        lua_setglobal(Lsetup, "print");
    }

    return *rtPtr;
}

// Runs the chunk and hands control straight back, leaving whatever it armed for `varnPoll` or `varnPollBudget` to drive.
RunResult WasmHost::loadChunk(const std::string& source)
{
    RunResult result;

    varn::runtime::Runtime& rt = ensureRuntime();
    std::string collected;
    std::string err;

    {
        PrintSinkScope sinkScope(&collected);
        result.ok = rt.runStringWithoutEventLoop(source, "=wasm", &err);
    }

    result.output = std::move(collected);
    if (!result.ok)
    {
        result.error = std::move(err);
    }

    return result;
}

// The page drives the runtime from its own loop, typically `requestAnimationFrame`, the same way a native app pumps it, and what Lua printed goes to the console.
bool WasmHost::poll()
{
    std::string collected;
    bool more = false;
    {
        PrintSinkScope sinkScope(&collected);
        more = ensureRuntime().poll();
    }

    if (!collected.empty())
    {
        emscripten::val::global("console").call<void>("log", collected);
    }

    return more;
}

// Advances the runtime for about the budget and answers what `varn_runtime_poll_budget` answers, with what Lua printed meanwhile, so the page decides when to poll again.
PollResult WasmHost::pollBudget(double budgetMilliseconds)
{
    PollResult result;
    const auto budget = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double, std::milli>(std::max(budgetMilliseconds, 0.0)));

    varn::runtime::EventLoop::PollResult polled;
    {
        PrintSinkScope sinkScope(&result.output);
        polled = ensureRuntime().poll(budget);
    }

    result.ran = polled.ran;
    result.pending = polled.pending;
    result.idleMilliseconds = static_cast<double>(polled.idleMilliseconds);
    return result;
}

// The page supplies a native capability the same way an iOS or Android host does, as a JSON in, JSON out function.
void WasmHost::registerHostFunction(const std::string& name, emscripten::val callback)
{
    // clang-format off
    ensureRuntime().registerHostFunction(name, [callback](const std::string& argument) -> std::string
    {
        emscripten::val answer = callback(argument);
        if (answer.isUndefined() || answer.isNull())
        {
            return std::string("null");
        }

        return answer.as<std::string>();
    });
    // clang-format on
}

bool WasmHost::emit(const std::string& name, const std::string& jsonArgument)
{
    ensureRuntime().emitHostEvent(name, jsonArgument);
    return true;
}

} // namespace varn::wasm

// The function `varnRunChunk` is written in JavaScript on top of these, in the file `src/wasm/varn_wasm_post.js`.
EMSCRIPTEN_BINDINGS(varn_wasm)
{
    emscripten::value_object<varn::wasm::RunResult>("RunResult")
        .field("ok", &varn::wasm::RunResult::ok)
        .field("output", &varn::wasm::RunResult::output)
        .field("error", &varn::wasm::RunResult::error);

    emscripten::value_object<varn::wasm::PollResult>("PollResult")
        .field("ran", &varn::wasm::PollResult::ran)
        .field("pending", &varn::wasm::PollResult::pending)
        .field("idleMilliseconds", &varn::wasm::PollResult::idleMilliseconds)
        .field("output", &varn::wasm::PollResult::output);

    emscripten::function("varnLoadChunk", &varn::wasm::WasmHost::loadChunk);
    emscripten::function("varnPoll", &varn::wasm::WasmHost::poll);
    emscripten::function("varnPollBudget", &varn::wasm::WasmHost::pollBudget);
    emscripten::function("varnRegister", &varn::wasm::WasmHost::registerHostFunction);
    emscripten::function("varnEmit", &varn::wasm::WasmHost::emit);
}
