#include "varn_ffi_dl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static char varn_dl_errbuf[512];

static void varn_dl_set_winerr(const char *ctx)
{
  DWORD e = GetLastError();
  if (ctx && *ctx) {
    snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "%s: Win32 error %lu", ctx, (unsigned long)e);
  } else {
    snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "Win32 error %lu", (unsigned long)e);
  }
}

const char *varn_ffi_dl_last_error(void)
{
  return varn_dl_errbuf[0] ? varn_dl_errbuf : "unknown error";
}

void *varn_ffi_dl_open(const char *path, int global_flag)
{
  (void)global_flag;
  varn_dl_errbuf[0] = '\0';
  if (!path) {
    return VARN_FFI_DL_DEFAULT;
  }
  SetLastError(0);
  HMODULE h = LoadLibraryA(path);
  if (!h) {
    varn_dl_set_winerr("LoadLibraryA");
    return NULL;
  }
  return (void *)h;
}

static FARPROC varn_dlsym_default(const char *name)
{
  static const char *const kMods[] = {
      NULL,
      "ucrtbase.dll",
      "msvcrt.dll",
      "vcruntime140.dll",
      "vcruntime140_1.dll",
      "msvcp140.dll",
      "kernel32.dll",
      "ntdll.dll",
  };
  for (size_t i = 0; i < sizeof(kMods) / sizeof(kMods[0]); i++) {
    HMODULE m = kMods[i] ? GetModuleHandleA(kMods[i]) : GetModuleHandleA(NULL);
    if (!m) {
      continue;
    }
    FARPROC p = GetProcAddress(m, name);
    if (p) {
      return p;
    }
  }
  return NULL;
}

void *varn_ffi_dl_sym(void *handle, const char *name)
{
  varn_dl_errbuf[0] = '\0';
  if (handle == VARN_FFI_DL_DEFAULT) {
    FARPROC p = varn_dlsym_default(name);
    if (!p) {
      snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "symbol not found: %s", name);
      return NULL;
    }
    return (void *)p;
  }
  FARPROC p = GetProcAddress((HMODULE)handle, name);
  if (!p) {
    varn_dl_set_winerr(name);
    return NULL;
  }
  return (void *)p;
}

void varn_ffi_dl_close(void *handle)
{
  if (!handle || handle == VARN_FFI_DL_DEFAULT) {
    return;
  }
  FreeLibrary((HMODULE)handle);
}

#else
#include <dlfcn.h>

static char varn_dl_errbuf[512];

const char *varn_ffi_dl_last_error(void)
{
  const char *e = dlerror();
  return (e && e[0]) ? e : varn_dl_errbuf;
}

void *varn_ffi_dl_open(const char *path, int global_flag)
{
  varn_dl_errbuf[0] = '\0';
  if (!path) {
    return VARN_FFI_DL_DEFAULT;
  }
  int flags = RTLD_LAZY | (global_flag ? RTLD_GLOBAL : RTLD_LOCAL);
  void *h = dlopen(path, flags);
  if (!h) {
    const char *e = dlerror();
    snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "%s", e ? e : "dlopen failed");
  } else {
    varn_dl_errbuf[0] = '\0';
  }
  return h;
}

void *varn_ffi_dl_sym(void *handle, const char *name)
{
  varn_dl_errbuf[0] = '\0';

  /* clear any stale error so a failing dlsym is reported and not confused with a previous one */
  dlerror();
  void *sym = dlsym(handle == VARN_FFI_DL_DEFAULT ? RTLD_DEFAULT : handle, name);
  if (!sym) {
    const char *e = dlerror();
    if (e) {
      snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "%s", e);
    }
  }
  return sym;
}

void varn_ffi_dl_close(void *handle)
{
  if (!handle || handle == VARN_FFI_DL_DEFAULT) {
    return;
  }
  dlclose(handle);
}

#endif
