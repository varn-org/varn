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

    VARN_API int varn_runtime_run_file(varn_runtime* runtime, const char* path);
    VARN_API int varn_runtime_run_string(varn_runtime* runtime, const char* source, const char* chunk_name);
    VARN_API void varn_runtime_stop(varn_runtime* runtime);
    VARN_API void varn_runtime_free(varn_runtime* runtime);
    VARN_API const char* varn_version(void);

#ifdef __cplusplus
}
#endif
