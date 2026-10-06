#pragma once

#include "varn_ffi_dl.h"

#include <stddef.h>

#define RTLD_LAZY 1
#define RTLD_LOCAL 2
#define RTLD_GLOBAL 4
#define RTLD_DEFAULT VARN_FFI_DL_DEFAULT

static inline void* dlopen(const char* path, int flags)
{
    return varn_ffi_dl_open(path, (flags & RTLD_GLOBAL) != 0);
}

static inline void* dlsym(void* handle, const char* name)
{
    return varn_ffi_dl_sym(handle, name);
}

static inline int dlclose(void* handle)
{
    varn_ffi_dl_close(handle);
    return 0;
}

static inline const char* dlerror(void)
{
    return varn_ffi_dl_last_error();
}
