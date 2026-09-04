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

    /* delivers an event to every lua handler registered for name through host.on, callable from any thread */
    VARN_API int varn_runtime_emit(varn_runtime* runtime, const char* name, const char* json_argument);

    /* a console level, matching the browser console functions: 0 log, 1 debug, 2 info, 3 warn, 4 error */
    typedef void (*varn_console_sink)(int level, const char* message, void* userdata);

    /* receives every line the engine writes, which already went to the platform console the host runs on.
       pass a null sink to stop receiving. the sink is process wide, as the console it mirrors is */
    VARN_API void varn_set_console(varn_console_sink sink, void* userdata);

    /* runs a chunk without entering the event loop, leaving whatever it armed for varn_runtime_poll to drive */
    VARN_API int varn_runtime_load_file(varn_runtime* runtime, const char* path);
    VARN_API int varn_runtime_load_string(varn_runtime* runtime, const char* source, const char* chunk_name);

    /* advances the runtime once without ever blocking, so an app drives it from its own run loop on its own thread.
       answers 1 while something can still make progress, 0 once nothing can, and 2 for a bad argument */
    VARN_API int varn_runtime_poll(varn_runtime* runtime);

    /* keeps the event loop running while the host still has work for it, so an app can wait for input instead of exiting */
    VARN_API int varn_runtime_retain(varn_runtime* runtime);

    /* gives one retain back, answering non-zero when there was none to give so an unbalanced call cannot hang the loop */
    VARN_API int varn_runtime_release(varn_runtime* runtime);

    VARN_API int varn_runtime_run_file(varn_runtime* runtime, const char* path);
    VARN_API int varn_runtime_run_string(varn_runtime* runtime, const char* source, const char* chunk_name);
    VARN_API void varn_runtime_stop(varn_runtime* runtime);
    VARN_API void varn_runtime_free(varn_runtime* runtime);
    VARN_API const char* varn_version(void);

#ifdef __cplusplus
}
#endif
