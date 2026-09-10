#include <varn/varn.h>

#include <stdio.h>

// a native capability the Lua script calls as host.greet(value); it receives the argument as json and returns json.
static const char* greet(const char* json_argument, void* userdata)
{
    (void)userdata;
    printf("[host] greet received: %s\n", json_argument);
    return "{\"message\":\"hello from the host\"}";
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
        "print('[lua] host replied: ' .. reply.message)\n";

    const int code = varn_runtime_run_string(runtime, script, "embedding-example");
    varn_runtime_free(runtime);
    return code;
}
