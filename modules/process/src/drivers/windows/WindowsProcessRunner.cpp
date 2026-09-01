#include "varn/process/ProcessRunner.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace varn::process
{

namespace
{
// bound the captured output so a child that streams without end cannot exhaust host memory
constexpr std::size_t kMaxCaptureBytes = 64 * 1024 * 1024;

class WindowsProcessHelpers
{
public:
    static std::wstring toWide(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return std::wstring();
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), size);
        return wide;
    }

    static std::string toUtf8(const wchar_t* wide, int wideLen)
    {
        if (wideLen <= 0)
        {
            return std::string();
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, wide, wideLen, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide, wideLen, utf8.data(), size, nullptr, nullptr);
        return utf8;
    }

    // reads until the write ends are closed, stopping once the combined capture reaches the cap
    static std::string drainPipe(HANDLE pipe, std::atomic<std::size_t>& captured)
    {
        std::string out;
        std::vector<char> chunk(65536);

        while (true)
        {
            DWORD got = 0;
            const BOOL ok = ReadFile(pipe, chunk.data(), static_cast<DWORD>(chunk.size()), &got, nullptr);
            if (!ok || got == 0)
            {
                break;
            }

            const std::size_t before = captured.fetch_add(static_cast<std::size_t>(got));
            const std::size_t room = before < kMaxCaptureBytes ? kMaxCaptureBytes - before : 0;
            out.append(chunk.data(), std::min(room, static_cast<std::size_t>(got)));

            if (static_cast<std::size_t>(got) >= room)
            {
                break;
            }
        }

        return out;
    }

    // killing cmd.exe alone leaves its command running with the inherited pipe ends, so the job is terminated when there is one
    static void killTree(HANDLE job, HANDLE fallback)
    {
        if (job != nullptr)
        {
            TerminateJobObject(job, 1);
            return;
        }

        TerminateProcess(fallback, 1);
    }

    static void closeHandle(HANDLE& handle)
    {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            handle = nullptr;
        }
    }
};
} // namespace

bool ProcessRunner::available()
{
    return true;
}

ProcessResult ProcessRunner::exec(const std::string& command, long long timeoutMs)
{
    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = static_cast<DWORD>(sizeof(inheritable));
    inheritable.bInheritHandle = TRUE;
    inheritable.lpSecurityDescriptor = nullptr;

    HANDLE outRead = nullptr;
    HANDLE outWrite = nullptr;
    HANDLE errRead = nullptr;
    HANDLE errWrite = nullptr;

    // serialize handle creation and process start so a concurrent spawn cannot inherit these write handles
    static std::mutex spawnMutex;
    PROCESS_INFORMATION process{};

    // terminating cmd.exe leaves the command it started alive, still holding the inherited pipe ends, so the child goes into a job that gives the deadline and the output cap a handle on the whole tree
    HANDLE job = CreateJobObjectW(nullptr, nullptr);

    {
        std::lock_guard<std::mutex> lock(spawnMutex);

        if (!CreatePipe(&outRead, &outWrite, &inheritable, 0) || !CreatePipe(&errRead, &errWrite, &inheritable, 0))
        {
            WindowsProcessHelpers::closeHandle(outRead);
            WindowsProcessHelpers::closeHandle(outWrite);
            WindowsProcessHelpers::closeHandle(errRead);
            WindowsProcessHelpers::closeHandle(errWrite);
            throw std::runtime_error("[ProcessRunner] A pipe could not be created.");
        }

        // keep the parent read ends out of child inheritance
        SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

        const std::wstring commandLine = L"cmd.exe /c " + WindowsProcessHelpers::toWide(command);
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = static_cast<DWORD>(sizeof(startup));
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = outWrite;
        startup.hStdError = errWrite;

        // the child starts suspended so it joins the job before it can spawn anything outside it
        const BOOL started = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
        if (!started)
        {
            WindowsProcessHelpers::closeHandle(job);
            WindowsProcessHelpers::closeHandle(outRead);
            WindowsProcessHelpers::closeHandle(outWrite);
            WindowsProcessHelpers::closeHandle(errRead);
            WindowsProcessHelpers::closeHandle(errWrite);
            throw std::runtime_error("[ProcessRunner] The process could not be started.");
        }

        // a host that already runs this process inside a job can refuse the assignment, and terminating an empty job would kill nothing, so the handle is dropped and the child is killed directly instead
        if (job != nullptr && !AssignProcessToJobObject(job, process.hProcess))
        {
            WindowsProcessHelpers::closeHandle(job);
        }

        ResumeThread(process.hThread);

        // close the parent copies of the write ends inside the lock so the reads terminate and no later spawn inherits them
        WindowsProcessHelpers::closeHandle(outWrite);
        WindowsProcessHelpers::closeHandle(errWrite);
    }

    ProcessResult result;

    // a synchronous ReadFile cannot be given a deadline, so the deadline is enforced on the child itself and the drains end when its pipes close
    std::atomic<bool> timedOut{false};
    std::thread watchdog;
    if (timeoutMs > 0)
    {
        // clang-format off
        watchdog = std::thread([&timedOut, handle = process.hProcess, tree = job, timeoutMs]
        {
            if (WaitForSingleObject(handle, static_cast<DWORD>(std::min<long long>(timeoutMs, MAXDWORD - 1))) == WAIT_TIMEOUT)
            {
                timedOut.store(true);
                WindowsProcessHelpers::killTree(tree, handle);
            }
        });
        // clang-format on
    }

    // drain stderr on a helper thread so a child that fills one pipe buffer while writing the other never deadlocks the parent
    std::string errData;
    std::atomic<std::size_t> captured{0};
    // clang-format off
    std::thread errReader([&errData, errRead, &captured]
    {
        // an escaping exception in a thread body would terminate the process, so a failed capture yields no stderr instead
        try { errData = WindowsProcessHelpers::drainPipe(errRead, captured); }
        catch (...) {}
    });
    // clang-format on

    // guard the main drain too so the reader thread is always joined even if the capture throws
    try
    {
        result.stdoutData = WindowsProcessHelpers::drainPipe(outRead, captured);
    }
    catch (...)
    {
    }

    errReader.join();
    if (watchdog.joinable())
    {
        watchdog.join();
    }

    result.stderrData = std::move(errData);
    result.timedOut = timedOut.load();

    WindowsProcessHelpers::closeHandle(outRead);
    WindowsProcessHelpers::closeHandle(errRead);

    // a child that overran the output cap is terminated so it cannot linger blocked on a full pipe
    if (captured.load() >= kMaxCaptureBytes)
    {
        WindowsProcessHelpers::killTree(job, process.hProcess);
    }

    WaitForSingleObject(process.hProcess, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    result.code = static_cast<int>(code);

    WindowsProcessHelpers::closeHandle(process.hProcess);
    WindowsProcessHelpers::closeHandle(process.hThread);
    WindowsProcessHelpers::closeHandle(job);

    return result;
}

std::optional<std::string> ProcessRunner::getenv(const std::string& name)
{
    const std::wstring wideName = WindowsProcessHelpers::toWide(name);

    // retry until the buffer fits since another thread can enlarge the variable between the size probe and the read
    for (;;)
    {
        const DWORD needed = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
        if (needed == 0)
        {
            return std::nullopt;
        }

        std::wstring buffer(needed, L'\0');
        const DWORD written = GetEnvironmentVariableW(wideName.c_str(), buffer.data(), needed);
        if (written == 0)
        {
            return std::nullopt;
        }

        if (written < needed)
        {
            return WindowsProcessHelpers::toUtf8(buffer.data(), static_cast<int>(written));
        }
    }
}

std::vector<std::pair<std::string, std::string>> ProcessRunner::environment()
{
    std::vector<std::pair<std::string, std::string>> entries;

    wchar_t* block = GetEnvironmentStringsW();
    if (block == nullptr)
    {
        return entries;
    }

    for (wchar_t* cursor = block; *cursor != L'\0';)
    {
        const std::wstring entry = cursor;
        cursor += entry.size() + 1;

        // skips the drive pseudo-variables that have no usable name
        const std::size_t equals = entry.find(L'=');
        if (equals == std::wstring::npos || equals == 0)
        {
            continue;
        }

        const std::string key = WindowsProcessHelpers::toUtf8(entry.data(), static_cast<int>(equals));
        const std::string value = WindowsProcessHelpers::toUtf8(entry.data() + equals + 1, static_cast<int>(entry.size() - equals - 1));
        entries.emplace_back(key, value);
    }

    FreeEnvironmentStringsW(block);
    return entries;
}

std::string ProcessRunner::cwd()
{
    const DWORD needed = GetCurrentDirectoryW(0, nullptr);
    if (needed == 0)
    {
        return std::string();
    }

    std::wstring buffer(needed, L'\0');
    const DWORD written = GetCurrentDirectoryW(needed, buffer.data());

    return WindowsProcessHelpers::toUtf8(buffer.data(), static_cast<int>(written));
}

} // namespace varn::process
