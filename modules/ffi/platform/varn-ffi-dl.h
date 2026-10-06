#pragma once

#include <stddef.h>
#include <stdint.h>

#define VARN_FFI_DL_DEFAULT ((void*)(intptr_t)-2)

void* varn_ffi_dl_open(const char* path, int global_flag);
void* varn_ffi_dl_sym(void* handle, const char* name);
void varn_ffi_dl_close(void* handle);
const char* varn_ffi_dl_last_error(void);
