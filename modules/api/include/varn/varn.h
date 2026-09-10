#pragma once

#if defined(_WIN32)
#if defined(VARN_SHARED)
#define VARN_API __declspec(dllexport)
#else
#define VARN_API
#endif
#else
#define VARN_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct varn_runtime varn_runtime;

    typedef const char* (*varn_host_function)(const char* json_argument, void* userdata);

    VARN_API varn_runtime* varn_runtime_new(void);

    VARN_API int varn_runtime_register(varn_runtime* runtime, const char* name, varn_host_function fn, void* userdata);

    /* Delivers an event to every Lua handler registered for `name` through `host.on`, callable from any thread. */
    VARN_API int varn_runtime_emit(varn_runtime* runtime, const char* name, const char* json_argument);

    /* A log level is 0 for debug, 1 for info, 2 for warn and 3 for error. A printed line arrives as info. */
    typedef void (*varn_console_sink)(int level, const char* message, void* userdata);

    /* Receives every line the engine writes, which already went to the console the platform provides.
       Pass a null sink to stop receiving.
       The sink is process wide, as the logger it mirrors is. */
    VARN_API void varn_set_console(varn_console_sink sink, void* userdata);

    /* Runs a chunk without entering the event loop, leaving whatever it armed for `varn_runtime_poll` to drive. */
    VARN_API int varn_runtime_load_file(varn_runtime* runtime, const char* path);
    VARN_API int varn_runtime_load_string(varn_runtime* runtime, const char* source, const char* chunk_name);

    /* Advances the runtime once without ever blocking, so an app drives it from its own run loop on its own thread.
       It answers 1 while something can still make progress, 0 once nothing can, and 2 for a bad argument. */
    VARN_API int varn_runtime_poll(varn_runtime* runtime);

    /* Called from any thread whenever work reaches the runtime from another thread, such as a promise a worker settled or an emitted event, so a host that sleeps in its own run loop knows to poll again.
       Pass a null function to stop, which returns once no call is still running. */
    typedef void (*varn_wake_function)(void* userdata);
    VARN_API void varn_runtime_set_wake(varn_runtime* runtime, varn_wake_function fn, void* userdata);

    /* Answers how many milliseconds the runtime can wait before it has work of its own: 0 while work is ready, the time to its next timer while one is armed, and -1 when only a wake can bring work. */
    VARN_API long long varn_runtime_idle(varn_runtime* runtime);

    /* Keeps the event loop running while the host still has work for it, so an app can wait for input instead of exiting. */
    VARN_API int varn_runtime_retain(varn_runtime* runtime);

    /* Gives one retain back, answering non-zero when there was none to give so an unbalanced call cannot hang the loop. */
    VARN_API int varn_runtime_release(varn_runtime* runtime);

    VARN_API int varn_runtime_run_file(varn_runtime* runtime, const char* path);
    VARN_API int varn_runtime_run_string(varn_runtime* runtime, const char* source, const char* chunk_name);
    VARN_API void varn_runtime_stop(varn_runtime* runtime);
    VARN_API void varn_runtime_free(varn_runtime* runtime);
    VARN_API const char* varn_version(void);

#ifdef __cplusplus
}
#endif
