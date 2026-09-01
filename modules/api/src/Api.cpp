#include "varn/varn.h"

#include "VarnVersion.h"
#include "varn/runtime/Runtime.h"

#include <string>
#include <vector>

extern "C"
{

    varn_runtime* varn_runtime_new(void)
    {
        try
        {
            auto* runtime = new varn::runtime::Runtime(std::vector<std::string>{"varn"});
            return reinterpret_cast<varn_runtime*>(runtime);
        }
        catch (...)
        {
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
            reinterpret_cast<varn::runtime::Runtime*>(runtime)->registerHostFunction(name, [fn, userdata](const std::string& argument) -> std::string
            {
                const char* result = fn(argument.c_str(), userdata);
                return result != nullptr ? std::string(result) : std::string("null");
            });
            // clang-format on
            return 0;
        }
        catch (...)
        {
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
            return reinterpret_cast<varn::runtime::Runtime*>(runtime)->runScript(path);
        }
        catch (...)
        {
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
            return reinterpret_cast<varn::runtime::Runtime*>(runtime)->runString(source, chunk_name ? chunk_name : "=(embedded)");
        }
        catch (...)
        {
            return 1;
        }
    }

    void varn_runtime_stop(varn_runtime* runtime)
    {
        if (runtime)
        {
            try
            {
                reinterpret_cast<varn::runtime::Runtime*>(runtime)->stop();
            }
            catch (...)
            {
            }
        }
    }

    void varn_runtime_free(varn_runtime* runtime)
    {
        try
        {
            delete reinterpret_cast<varn::runtime::Runtime*>(runtime);
        }
        catch (...)
        {
        }
    }

    const char* varn_version(void)
    {
        return VARN_VERSION_STRING;
    }

} // extern "C"
