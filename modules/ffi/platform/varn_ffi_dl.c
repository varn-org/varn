#include "varn_ffi_dl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static char varn_dl_errbuf[512];

/* Keeps the last error of the system with the text the system gives for it, in UTF-8. */
static void varn_dl_set_winerr(void)
{
    DWORD code = GetLastError();
    wchar_t text[256];
    char reason[384] = "";
    DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0, text, sizeof(text) / sizeof(text[0]), NULL);

    while (length > 0 && (text[length - 1] == L'\r' || text[length - 1] == L'\n'))
    {
        length--;
    }

    if (length > 0)
    {
        int written = WideCharToMultiByte(CP_UTF8, 0, text, (int)length, reason, (int)sizeof(reason) - 1, NULL, NULL);
        reason[written > 0 ? written : 0] = '\0';
    }

    snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "Windows error %lu: %s", (unsigned long)code, reason);
}

/* Answers whether a path names a drive or a network share, which is what lets the loader search the folder of the library for what it needs. */
static int varn_dl_is_absolute(const wchar_t* path)
{
    const int separator = path[0] == L'\\' || path[0] == L'/';
    const int drive = (path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z');

    if (separator && (path[1] == L'\\' || path[1] == L'/'))
    {
        return 1;
    }

    return drive && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
}

const char* varn_ffi_dl_last_error(void)
{
    return varn_dl_errbuf[0] ? varn_dl_errbuf : "Unknown error";
}

/* Loads a library from a UTF-8 path, so a name outside the code page of the system still loads. */
void* varn_ffi_dl_open(const char* path, int global_flag)
{
    (void)global_flag;
    varn_dl_errbuf[0] = '\0';
    if (!path)
    {
        return VARN_FFI_DL_DEFAULT;
    }

    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (length == 0)
    {
        varn_dl_set_winerr();
        return NULL;
    }

    wchar_t* wide = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)length);
    if (!wide)
    {
        snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "The path could not be converted to UTF-16, since memory ran out.");
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, length);

    /* An absolute path also lets the loader find the libraries it depends on beside it. */
    const DWORD flags = varn_dl_is_absolute(wide) ? LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR : 0;
    HMODULE h = LoadLibraryExW(wide, NULL, flags);
    if (!h)
    {
        varn_dl_set_winerr();
    }

    free(wide);
    return (void*)h;
}

static FARPROC varn_dlsym_default(const char* name)
{
    static const char* const kMods[] = {
        NULL,
        "ucrtbase.dll",
        "msvcrt.dll",
        "vcruntime140.dll",
        "vcruntime140_1.dll",
        "msvcp140.dll",
        "kernel32.dll",
        "ntdll.dll",
    };
    for (size_t i = 0; i < sizeof(kMods) / sizeof(kMods[0]); i++)
    {
        HMODULE m = kMods[i] ? GetModuleHandleA(kMods[i]) : GetModuleHandleA(NULL);
        if (!m)
        {
            continue;
        }
        FARPROC p = GetProcAddress(m, name);
        if (p)
        {
            return p;
        }
    }
    return NULL;
}

void* varn_ffi_dl_sym(void* handle, const char* name)
{
    varn_dl_errbuf[0] = '\0';
    if (handle == VARN_FFI_DL_DEFAULT)
    {
        FARPROC p = varn_dlsym_default(name);
        if (!p)
        {
            snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "Symbol not found: %s", name);
            return NULL;
        }
        return (void*)p;
    }
    FARPROC p = GetProcAddress((HMODULE)handle, name);
    if (!p)
    {
        varn_dl_set_winerr();
        return NULL;
    }
    return (void*)p;
}

void varn_ffi_dl_close(void* handle)
{
    if (!handle || handle == VARN_FFI_DL_DEFAULT)
    {
        return;
    }
    FreeLibrary((HMODULE)handle);
}

#else
#include <dlfcn.h>

static char varn_dl_errbuf[512];

const char* varn_ffi_dl_last_error(void)
{
    const char* e = dlerror();
    return (e && e[0]) ? e : varn_dl_errbuf;
}

void* varn_ffi_dl_open(const char* path, int global_flag)
{
    varn_dl_errbuf[0] = '\0';
    if (!path)
    {
        return VARN_FFI_DL_DEFAULT;
    }
    int flags = RTLD_LAZY | (global_flag ? RTLD_GLOBAL : RTLD_LOCAL);
    void* h = dlopen(path, flags);
    if (!h)
    {
        const char* e = dlerror();
        snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "%s", e ? e : "The \"dlopen\" call failed");
    }
    else
    {
        varn_dl_errbuf[0] = '\0';
    }
    return h;
}

void* varn_ffi_dl_sym(void* handle, const char* name)
{
    varn_dl_errbuf[0] = '\0';

    /* Clear any stale error so a failing `dlsym` is reported and not confused with a previous one. */
    dlerror();
    void* sym = dlsym(handle == VARN_FFI_DL_DEFAULT ? RTLD_DEFAULT : handle, name);
    if (!sym)
    {
        const char* e = dlerror();
        if (e)
        {
            snprintf(varn_dl_errbuf, sizeof varn_dl_errbuf, "%s", e);
        }
    }
    return sym;
}

void varn_ffi_dl_close(void* handle)
{
    if (!handle || handle == VARN_FFI_DL_DEFAULT)
    {
        return;
    }
    dlclose(handle);
}

#endif
