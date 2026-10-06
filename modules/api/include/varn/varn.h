#pragma once

#include <stddef.h>

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

    /* The most threads each worker pool of a runtime starts, where zero keeps the default: a thread per core for `task_threads` and a high count for `io_threads`, which serves blocking work such as files, processes and the HTTP client. A pool starts its threads only as its jobs need them and ends the ones that stay idle. */
    typedef struct varn_runtime_options
    {
        unsigned task_threads;
        unsigned io_threads;
    } varn_runtime_options;

    /* Creates a runtime whose pools have the limits `options` names, so a host fits them to its device, and answers null for a null `options`. */
    VARN_API varn_runtime* varn_runtime_new_with_options(const varn_runtime_options* options);

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

    /* What one budgeted poll did: whether any job, timer or socket callback ran, whether anything can still make progress, and the milliseconds the host can sleep as `varn_runtime_idle` answers them. */
    typedef struct varn_poll_result
    {
        int ran;
        int pending;
        long long idle_milliseconds;
    } varn_poll_result;

    /* Advances the runtime without blocking, repeating passes of its jobs, timers and sockets while they make progress and the budget of nanoseconds lasts, so a host bounds the time one frame gives the runtime.
       It answers 1 while something can still make progress, 0 once nothing can, and 2 for a bad argument, and fills `result` when it is not null. */
    VARN_API int varn_runtime_poll_budget(varn_runtime* runtime, long long budget_nanoseconds, varn_poll_result* result);

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

    /* A data view is memory that the host and Lua share by reference instead of copying it into JSON: an address, a size and an id that is never reused in the process.
       Every holder owns one reference, from any thread, and the memory is freed or handed back once the last reference is released.
       JSON names a view with the token {"$view": id, "pointer": "0x...", "size": n}, which never owns it, so whoever sends a token keeps the view alive until the call that carries it returns. */
    typedef struct varn_dataview varn_dataview;

    /* Hands memory that a view wrapped back to its owner, on the thread that released the last reference. */
    typedef void (*varn_dataview_release_function)(void* data, void* userdata);

    /* Allocates a view of `size` zeroed bytes owned by the runtime, answering null when the memory cannot be allocated. */
    VARN_API varn_dataview* varn_dataview_new(size_t size);

    /* Wraps memory the host owns, such as a camera frame, and calls `release` once the last reference ends, answering null for a null `data` with a size. */
    VARN_API varn_dataview* varn_dataview_wrap(void* data, size_t size, varn_dataview_release_function release, void* userdata);

    /* Takes a reference to the live view that a token names, answering null when no live view has that id, pointer and size. */
    VARN_API varn_dataview* varn_dataview_acquire(unsigned long long id, const void* pointer, size_t size);

    VARN_API void varn_dataview_retain(varn_dataview* view);
    VARN_API void varn_dataview_release(varn_dataview* view);
    VARN_API void* varn_dataview_data(const varn_dataview* view);
    VARN_API size_t varn_dataview_size(const varn_dataview* view);
    VARN_API unsigned long long varn_dataview_id(const varn_dataview* view);

    /* Writes the JSON token of the view and its terminating null into `buffer` when it fits, and answers the length of the token without the null, as `snprintf` does. */
    VARN_API size_t varn_dataview_token(const varn_dataview* view, char* buffer, size_t capacity);

    /* Gives one reference back on the loop thread once the work the loop runs now is done, so a host function answers with a view it made for the answer and lets it go before it returns. */
    VARN_API int varn_runtime_release_dataview(varn_runtime* runtime, varn_dataview* view);

#ifdef __cplusplus
}
#endif
