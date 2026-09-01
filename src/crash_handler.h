#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>

#if defined(_WIN32)

// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on

#include <cstdint>

#else

#include <csignal>
#include <cstring>
#include <unistd.h>

#if __has_include(<execinfo.h>)
#include <execinfo.h>
#define VARN_HAS_EXECINFO 1
#endif

#endif

namespace varn::diagnostics
{

class CrashHandler
{
public:
    CrashHandler() = delete;

    static void install();

private:
    static void printBacktrace();
#if defined(_WIN32)
    static const char* exceptionName(DWORD code);
    static LONG WINAPI crashFilter(EXCEPTION_POINTERS* info);
#else
    static const char* signalName(int sig);
    static void crashSignalHandler(int sig);
#endif

    static void printActiveException();
    static void terminateHandler();
};

#if defined(_WIN32)

inline const char* CrashHandler::exceptionName(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        return "access violation";
    case EXCEPTION_STACK_OVERFLOW:
        return "stack overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "integer divide by zero";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "privileged instruction";
    case EXCEPTION_IN_PAGE_ERROR:
        return "in-page i/o error";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "datatype misalignment";
    case 0xC0000374:
        return "heap corruption";
    case 0xC0000409:
        return "stack buffer overrun or fast-fail";
    default:
        return "unknown exception";
    }
}

inline void CrashHandler::printBacktrace()
{
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (!SymInitialize(process, nullptr, TRUE))
    {
        return;
    }

    void* frames[30];
    const USHORT count = CaptureStackBackTrace(0, 30, frames, nullptr);

    char storage[sizeof(SYMBOL_INFO) + 256] = {0};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    for (USHORT i = 0; i < count; ++i)
    {
        const DWORD64 address = static_cast<DWORD64>(reinterpret_cast<uintptr_t>(frames[i]));
        DWORD64 symbolOffset = 0;

        if (SymFromAddr(process, address, &symbolOffset, symbol))
        {
            IMAGEHLP_LINE64 line = {0};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineOffset = 0;

            if (SymGetLineFromAddr64(process, address, &lineOffset, &line))
            {
                fprintf(stderr, "  #%2u %s + 0x%llx  (%s:%lu)\n", i, symbol->Name,
                        static_cast<unsigned long long>(symbolOffset), line.FileName,
                        static_cast<unsigned long>(line.LineNumber));
            }
            else
            {
                fprintf(stderr, "  #%2u %s + 0x%llx\n", i, symbol->Name,
                        static_cast<unsigned long long>(symbolOffset));
            }
        }
        else
        {
            fprintf(stderr, "  #%2u %p\n", i, frames[i]);
        }
    }

    SymCleanup(process);
}

inline LONG WINAPI CrashHandler::crashFilter(EXCEPTION_POINTERS* info)
{
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    const DWORD code = record->ExceptionCode;

    fprintf(stderr, "\n[CrashHandler] Unhandled exception 0x%08lX (%s) at %p.\n", static_cast<unsigned long>(code),
            exceptionName(code), record->ExceptionAddress);

    if (code == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
    {
        const ULONG_PTR operation = record->ExceptionInformation[0];
        const char* verb = operation == 0 ? "reading" : (operation == 1 ? "writing" : "executing");
        fprintf(stderr, "[CrashHandler] While %s address 0x%llx.\n", verb,
                static_cast<unsigned long long>(record->ExceptionInformation[1]));
    }

    fflush(stderr);

    if (code != EXCEPTION_STACK_OVERFLOW)
    {
        printBacktrace();
        fflush(stderr);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

#else

inline void CrashHandler::printBacktrace()
{
#if defined(VARN_HAS_EXECINFO)
    // backtrace_symbols_fd is async-signal-safe and backtrace is pre-warmed in install so its first-use allocation is already done, unlike backtrace_symbols which is never called here
    void* frames[64];
    const int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
#endif
}

inline const char* CrashHandler::signalName(int sig)
{
    switch (sig)
    {
    case SIGSEGV:
        return "segmentation fault";
    case SIGBUS:
        return "bus error";
    case SIGFPE:
        return "floating-point exception";
    case SIGILL:
        return "illegal instruction";
    default:
        return "unknown signal";
    }
}

inline void CrashHandler::crashSignalHandler(int sig)
{
    // only async-signal-safe calls are allowed here since this runs inside a fatal fault
    const char* prefix = "\n[CrashHandler] Fatal signal: ";
    const char* name = signalName(sig);

    [[maybe_unused]] ssize_t written;
    written = ::write(STDERR_FILENO, prefix, std::strlen(prefix));
    written = ::write(STDERR_FILENO, name, std::strlen(name));
    written = ::write(STDERR_FILENO, "\n", 1);

    printBacktrace();

    // restore the default action and re-raise so the process terminates with the faulting signal
    signal(sig, SIG_DFL);
    raise(sig);
}

#endif // _WIN32

inline void CrashHandler::printActiveException()
{
    if (std::exception_ptr current = std::current_exception())
    {
        try
        {
            std::rethrow_exception(current);
        }
        catch (const std::exception& ex)
        {
            fprintf(stderr, "[CrashHandler] Unhandled C++ exception: %s.\n", ex.what());
        }
        catch (...)
        {
            fprintf(stderr, "[CrashHandler] Unhandled non-standard C++ exception.\n");
        }
    }
    else
    {
        fprintf(stderr, "[CrashHandler] Terminate with no active exception (often a longjmp across C++ frames).\n");
    }
}

inline void CrashHandler::terminateHandler()
{
    fprintf(stderr, "\n[CrashHandler] Triggered by std::terminate.\n");
    printActiveException();
    fflush(stderr);

    printBacktrace();
    fflush(stderr);

    std::abort();
}

inline void CrashHandler::install()
{
    std::set_terminate(&CrashHandler::terminateHandler);

#if defined(_WIN32)
    ULONG reserve = 65536;
    SetThreadStackGuarantee(&reserve);
    SetUnhandledExceptionFilter(&CrashHandler::crashFilter);
#else
    // run the handler on a dedicated stack so a stack-overflow fault can still be reported
    static char alternateStack[65536];
    stack_t altStack{};
    altStack.ss_sp = alternateStack;
    altStack.ss_size = sizeof(alternateStack);
    altStack.ss_flags = 0;
    sigaltstack(&altStack, nullptr);

#if defined(VARN_HAS_EXECINFO)
    // warm the unwinder now so the in-handler backtrace does not have to dlopen or allocate during a fatal fault
    void* warmup[1];
    (void)backtrace(warmup, 1);
#endif

    struct sigaction action{};
    action.sa_handler = &CrashHandler::crashSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_ONSTACK;
    for (const int fault : {SIGSEGV, SIGBUS, SIGFPE, SIGILL})
    {
        sigaction(fault, &action, nullptr);
    }
#endif
}

} // namespace varn::diagnostics
