#include "varn/varn.h"

#include "VarnVersion.h"
#include "varn/dataview/DataView.h"
#include "varn/log/Log.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/Scheduler.h"

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
// The runtime of the C API, a Lua runtime on a scheduler of its own, which the handle owns and frees together.
class EmbeddedRuntime
{
public:
    explicit EmbeddedRuntime(varn::runtime::Scheduler::Options options)
        : scheduler(options)
        , runtime(scheduler, std::vector<std::string>{"varn"})
    {
    }

    static EmbeddedRuntime* from(varn_runtime* handle) { return reinterpret_cast<EmbeddedRuntime*>(handle); }

    varn::runtime::Scheduler scheduler;
    varn::runtime::Runtime runtime;
};
} // namespace

extern "C"
{

    varn_runtime* varn_runtime_new(void)
    {
        try
        {
            auto* embedded = new EmbeddedRuntime({});
            return reinterpret_cast<varn_runtime*>(embedded);
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_new\" failed.");
            return nullptr;
        }
    }

    varn_runtime* varn_runtime_new_with_options(const varn_runtime_options* options)
    {
        if (!options)
        {
            return nullptr;
        }

        try
        {
            auto* embedded = new EmbeddedRuntime({options->task_threads, options->io_threads});
            return reinterpret_cast<varn_runtime*>(embedded);
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_new_with_options\" failed.");
            return nullptr;
        }
    }

    int varn_runtime_register(varn_runtime* runtime, const char* name, varn_host_function fn, void* userdata)
    {
        if (!runtime || !name || !fn)
        {
            return 2;
        }

        try
        {
            // clang-format off
            EmbeddedRuntime::from(runtime)->runtime.registerHostFunction(name, [fn, userdata](const std::string& argument) -> std::string
            {
                const char* result = fn(argument.c_str(), userdata);
                return result != nullptr ? std::string(result) : std::string("null");
            });
            // clang-format on
            return 0;
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_register\" failed.");
            return 1;
        }
    }

    int varn_runtime_emit(varn_runtime* runtime, const char* name, const char* json_argument)
    {
        if (!runtime || !name)
        {
            return 2;
        }

        try
        {
            EmbeddedRuntime::from(runtime)->runtime.emitHostEvent(name, json_argument != nullptr ? json_argument : "null");
            return 0;
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_emit\" failed.");
            return 1;
        }
    }

    void varn_set_console(varn_console_sink sink, void* userdata)
    {
        if (sink == nullptr)
        {
            varn::log::Log::setSink({});
            return;
        }

        // clang-format off
        varn::log::Log::setSink([sink, userdata](varn::log::Level level, std::string_view message)
        {
            sink(static_cast<int>(level), std::string(message).c_str(), userdata);
        });
        // clang-format on
    }

    int varn_runtime_load_file(varn_runtime* runtime, const char* path)
    {
        if (!runtime || !path)
        {
            return 2;
        }

        try
        {
            return EmbeddedRuntime::from(runtime)->runtime.loadFile(path);
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_load_file\" failed.");
            return 1;
        }
    }

    int varn_runtime_load_string(varn_runtime* runtime, const char* source, const char* chunk_name)
    {
        if (!runtime || !source)
        {
            return 2;
        }

        try
        {
            return EmbeddedRuntime::from(runtime)->runtime.loadString(source, chunk_name ? chunk_name : "=(embedded)");
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_load_string\" failed.");
            return 1;
        }
    }

    int varn_runtime_poll(varn_runtime* runtime)
    {
        if (!runtime)
        {
            return 2;
        }

        try
        {
            return EmbeddedRuntime::from(runtime)->scheduler.poll() ? 1 : 0;
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_poll\" failed.");
            return 3;
        }
    }

    int varn_runtime_poll_budget(varn_runtime* runtime, long long budget_nanoseconds, varn_poll_result* result)
    {
        if (!runtime || budget_nanoseconds < 0)
        {
            return 2;
        }

        varn::runtime::EventLoop::PollResult polled;
        try
        {
            polled = EmbeddedRuntime::from(runtime)->scheduler.poll(std::chrono::nanoseconds(budget_nanoseconds));
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_poll_budget\" failed.");
            return 3;
        }

        if (result != nullptr)
        {
            result->ran = polled.ran ? 1 : 0;
            result->pending = polled.pending ? 1 : 0;
            result->idle_milliseconds = polled.idleMilliseconds;
        }

        return polled.pending ? 1 : 0;
    }

    void varn_runtime_set_wake(varn_runtime* runtime, varn_wake_function fn, void* userdata)
    {
        if (!runtime)
        {
            return;
        }

        varn::runtime::EventLoop::WakeHandler handler;
        if (fn != nullptr)
        {
            // clang-format off
            handler = [fn, userdata] { fn(userdata); };
            // clang-format on
        }

        try
        {
            EmbeddedRuntime::from(runtime)->scheduler.mainLoop().setWakeHandler(std::move(handler));
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_set_wake\" failed.");
        }
    }

    long long varn_runtime_idle(varn_runtime* runtime)
    {
        if (!runtime)
        {
            return -1;
        }

        try
        {
            EmbeddedRuntime* embedded = EmbeddedRuntime::from(runtime);
            return embedded->runtime.stopped() ? -1 : embedded->scheduler.mainLoop().idleMilliseconds();
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_idle\" failed.");
            return 0;
        }
    }

    int varn_runtime_retain(varn_runtime* runtime)
    {
        if (!runtime)
        {
            return 2;
        }

        try
        {
            EmbeddedRuntime::from(runtime)->scheduler.mainLoop().retain();
            return 0;
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_retain\" failed.");
            return 1;
        }
    }

    int varn_runtime_release(varn_runtime* runtime)
    {
        if (!runtime)
        {
            return 2;
        }

        // An unbalanced release would otherwise leave the loop holding a retain nobody can ever give back.
        try
        {
            return EmbeddedRuntime::from(runtime)->scheduler.mainLoop().release() ? 0 : 1;
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_release\" failed.");
            return 1;
        }
    }

    int varn_runtime_run_file(varn_runtime* runtime, const char* path)
    {
        if (!runtime || !path)
        {
            return 2;
        }

        try
        {
            return EmbeddedRuntime::from(runtime)->runtime.runScript(path);
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_run_file\" failed.");
            return 1;
        }
    }

    int varn_runtime_run_string(varn_runtime* runtime, const char* source, const char* chunk_name)
    {
        if (!runtime || !source)
        {
            return 2;
        }

        try
        {
            return EmbeddedRuntime::from(runtime)->runtime.runString(source, chunk_name ? chunk_name : "=(embedded)");
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_run_string\" failed.");
            return 1;
        }
    }

    void varn_runtime_stop(varn_runtime* runtime)
    {
        if (runtime)
        {
            try
            {
                EmbeddedRuntime::from(runtime)->runtime.stop();
            }
            catch (...)
            {
                varn::log::Log::failure("The call \"varn_runtime_stop\" failed.");
            }
        }
    }

    void varn_runtime_free(varn_runtime* runtime)
    {
        try
        {
            delete EmbeddedRuntime::from(runtime);
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_free\" failed.");
        }
    }

    int varn_runtime_release_dataview(varn_runtime* runtime, varn_dataview* view)
    {
        if (!runtime || !view)
        {
            return 2;
        }

        // The job owns the reference, so it is given back even when the loop is destroyed before the job runs.
        try
        {
            // clang-format off
            EmbeddedRuntime::from(runtime)->scheduler.mainLoop().post([held = varn::dataview::DataView::adopt(view)]
            {
            });
            // clang-format on
            return 0;
        }
        catch (...)
        {
            varn::log::Log::failure("The call \"varn_runtime_release_dataview\" failed.");
            return 1;
        }
    }

    const char* varn_version(void)
    {
        return VARN_VERSION_STRING;
    }

} // extern "C"
