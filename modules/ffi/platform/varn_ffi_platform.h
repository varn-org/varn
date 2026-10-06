#pragma once

#include <errno.h>

#if defined(_WIN32)
#include <malloc.h>
#define varn_ffi_alloca(x) _alloca(x)
#else
#include <alloca.h>
#define varn_ffi_alloca(x) alloca(x)
#endif
