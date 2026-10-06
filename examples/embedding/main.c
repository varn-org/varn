#include <varn/varn.h>

#include <stdio.h>

// A native capability the Lua script calls as `host.greet(value)`. It receives the argument as JSON and returns JSON.
static const char* greet(const char* json_argument, void* userdata)
{
    (void)userdata;
    printf("[host] The \"greet\" call received: %s\n", json_argument);
    return "{\"message\":\"Hello from the host\"}";
}

int main(void)
{
    printf("varn %s\n", varn_version());

    varn_runtime* runtime = varn_runtime_new();
    if (runtime == NULL)
    {
        return 1;
    }

    varn_runtime_register(runtime, "greet", greet, NULL);

    const char* script =
        "local reply = host.greet({ name = 'world' })\n"
        "print('[lua] The host replied: ' .. reply.message)\n";

    const int code = varn_runtime_run_string(runtime, script, "embedding-example");
    varn_runtime_free(runtime);
    return code;
}
