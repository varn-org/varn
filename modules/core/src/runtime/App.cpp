#include "varn/runtime/App.h"
#include "varn/log/Log.h"
#include "varn/runtime/Runtime.h"
#include "varn/runtime/Scheduler.h"

#include "VarnVersion.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if !defined(_WIN32) && !defined(VARN_NO_FORK)
#include <cerrno>
#include <csignal>
#include <ctime>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(_WIN32) && !defined(VARN_NO_FORK)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#endif

namespace varn::runtime
{

#if !defined(VARN_NO_FORK)
namespace
{
// A worker that dies sooner than this after it started waits out the rest before it is restarted, so a crash loop cannot become a storm of new processes.
constexpr long long kWorkerMinLifetimeMs = 1000;
} // namespace
#endif

#if !defined(_WIN32) && !defined(VARN_NO_FORK)
namespace
{
volatile sig_atomic_t gWorkerShutdown = 0;

class PosixWorkerHelpers
{
public:
    static void onWorkerSignal(int)
    {
        gWorkerShutdown = 1;
    }

    static void resetChildSignals()
    {
        // Restore default signal handlers so a worker terminates on the parent's shutdown signal.
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
    }
};
} // namespace
#endif

#if defined(_WIN32) && !defined(VARN_NO_FORK)
namespace
{
// The completion port the supervisor waits on, which the console handler and the waits of the workers post to from other threads.
std::atomic<HANDLE> gWorkerPort{nullptr};

class WinWorkerHelpers
{
public:
    struct Worker
    {
        HANDLE process = nullptr;
        HANDLE wait = nullptr;
        std::chrono::steady_clock::time_point startedAt;
    };

    // A process handle is never zero, so the key zero tells the supervisor to stop.
    static constexpr ULONG_PTR kShutdownKey = 0;

    static BOOL WINAPI onWorkerConsoleEvent(DWORD)
    {
        // Any console control event asks the supervisor to stop restarting and tear the workers down.
        const HANDLE port = gWorkerPort.load();
        if (port != nullptr)
        {
            PostQueuedCompletionStatus(port, 0, kShutdownKey, nullptr);
        }

        return TRUE;
    }

    // Runs on a thread of the system pool when a worker exits and hands its process handle to the supervisor.
    static VOID CALLBACK onWorkerExit(PVOID process, BOOLEAN)
    {
        PostQueuedCompletionStatus(gWorkerPort.load(), 0, reinterpret_cast<ULONG_PTR>(process), nullptr);
    }

    static HANDLE spawnWorker(const std::wstring& commandLine)
    {
        STARTUPINFOW startup{};
        startup.cb = static_cast<DWORD>(sizeof(startup));

        // Inherit the supervisor's standard streams so redirected stdout and stderr reach the workers like a POSIX `fork` does.
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        for (HANDLE stream : {startup.hStdInput, startup.hStdOutput, startup.hStdError})
        {
            if (stream != nullptr && stream != INVALID_HANDLE_VALUE)
            {
                SetHandleInformation(stream, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            }
        }

        PROCESS_INFORMATION process{};
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process))
        {
            return nullptr;
        }

        CloseHandle(process.hThread);
        return process.hProcess;
    }

    // Starts a worker and watches for its exit, so no thread of the supervisor polls the workers.
    static bool startWorker(const std::wstring& commandLine, std::vector<Worker>& workers)
    {
        const HANDLE process = spawnWorker(commandLine);
        if (process == nullptr)
        {
            return false;
        }

        HANDLE wait = nullptr;
        if (!RegisterWaitForSingleObject(&wait, process, &WinWorkerHelpers::onWorkerExit, process, INFINITE, WT_EXECUTEONLYONCE))
        {
            TerminateProcess(process, 1);
            WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
            return false;
        }

        workers.push_back({process, wait, std::chrono::steady_clock::now()});
        return true;
    }

    static void stopWorker(const Worker& worker)
    {
        UnregisterWaitEx(worker.wait, INVALID_HANDLE_VALUE);
        TerminateProcess(worker.process, 0);
        WaitForSingleObject(worker.process, INFINITE);
        CloseHandle(worker.process);
    }

    static void clearWorkerChildMarker()
    {
        // Drop the recursion guard from the child environment so `os.getenv` never exposes the supervisor detail.
        _putenv("VARN_WORKER_CHILD=");
        SetEnvironmentVariableW(L"VARN_WORKER_CHILD", nullptr);
    }
};
} // namespace
#endif

bool App::isEvalFlag(std::string_view flag)
{
    return flag == "-e" || flag == "--eval";
}

bool App::isVersionFlag(std::string_view flag)
{
    return flag == "-v" || flag == "--version";
}

bool App::isHelpFlag(std::string_view flag)
{
    return flag == "-h" || flag == "--help";
}

// Usage goes to `stdout` so it can be piped, unlike the diagnostics the logger writes.
void App::printUsage()
{
    std::printf("varn %s\n\n", VARN_VERSION_STRING);
    std::printf("Usage:\n");
    std::printf("  varn <script.lua> [arguments...]   Run a script\n");
    std::printf("  varn -e <source> [arguments...]    Run an inline source string\n");
    std::printf("  varn --version                     Print the version\n");
    std::printf("  varn --help                        Print this message\n\n");
    std::printf("Environment:\n");
    std::printf("  VARN_WORKERS   Number of worker processes to supervise (default 1)\n");
}

// Reads the count of worker processes, which is one when the variable is unset, and refuses a value that is not a whole number from 1 to 1024.
int App::workerCount()
{
    const char* value = std::getenv("VARN_WORKERS");
    if (!value)
    {
        return 1;
    }

    constexpr int kMaxWorkers = 1024;
    const std::string_view text(value);
    int count = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
    if (error != std::errc() || end != text.data() + text.size() || count < 1 || count > kMaxWorkers)
    {
        throw std::invalid_argument("The variable \"VARN_WORKERS\" must be a whole number from 1 to " + std::to_string(kMaxWorkers) + ", not \"" + std::string(text) + "\".");
    }

    return count;
}

#if !defined(_WIN32) && !defined(VARN_NO_FORK)
int App::superviseWorkers(int count, const std::function<int()>& runChild)
{
    std::vector<pid_t> workers;
    std::vector<std::chrono::steady_clock::time_point> startedAt;
    workers.reserve(static_cast<std::size_t>(count));
    startedAt.reserve(static_cast<std::size_t>(count));

    // Install handlers without `SA_RESTART` before the first fork so a signal during startup cannot orphan workers, and `waitpid` returns `EINTR` on shutdown.
    struct sigaction action = {};
    action.sa_handler = PosixWorkerHelpers::onWorkerSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);

    for (int i = 0; i < count; ++i)
    {
        const pid_t pid = fork();
        if (pid < 0)
        {
            log::Log::error("Failed to spawn a worker process.");
            break;
        }

        if (pid == 0)
        {
            PosixWorkerHelpers::resetChildSignals();
            _exit(runChild());
        }

        workers.push_back(pid);
        startedAt.push_back(std::chrono::steady_clock::now());
    }

    if (workers.empty())
    {
        log::Log::error("No worker process could be started.");
        return 1;
    }

    // Restart a worker that dies abnormally and stop on the first shutdown signal, leaving a clean exit alone.
    while (!gWorkerShutdown)
    {
        int status = 0;
        const pid_t dead = ::waitpid(-1, &status, 0);
        if (dead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            break;
        }

        if (gWorkerShutdown)
        {
            break;
        }

        const auto slot = std::find(workers.begin(), workers.end(), dead);
        if (slot == workers.end())
        {
            continue;
        }

        const std::size_t index = static_cast<std::size_t>(slot - workers.begin());

        // A worker that finished cleanly is not resurrected, and the supervisor exits once none remain.
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            workers.erase(slot);
            startedAt.erase(startedAt.begin() + static_cast<std::ptrdiff_t>(index));
            if (workers.empty())
            {
                break;
            }

            continue;
        }

        // A worker that died almost immediately backs off before respawning so a crash loop cannot become a fork storm.
        const long long livedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startedAt[index]).count();
        if (livedMs < kWorkerMinLifetimeMs)
        {
            const long long backoffMs = kWorkerMinLifetimeMs - livedMs;
            struct timespec pause = {static_cast<time_t>(backoffMs / 1000), static_cast<long>((backoffMs % 1000) * 1000000)};
            ::nanosleep(&pause, nullptr);
            if (gWorkerShutdown)
            {
                break;
            }
        }

        const pid_t replacement = fork();
        if (replacement == 0)
        {
            PosixWorkerHelpers::resetChildSignals();
            _exit(runChild());
        }

        if (replacement > 0)
        {
            *slot = replacement;
            startedAt[index] = std::chrono::steady_clock::now();
        }
        else
        {
            log::Log::error("Failed to restart a worker process.");
            workers.erase(slot);
            startedAt.erase(startedAt.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    for (const pid_t pid : workers)
    {
        ::kill(pid, SIGTERM);
    }

    for (const pid_t pid : workers)
    {
        int status = 0;
        ::waitpid(pid, &status, 0);
    }

    return 0;
}
#endif

#if defined(_WIN32) && !defined(VARN_NO_FORK)
// Windows has no `fork`, so each worker is a relaunch of this executable flagged so it runs the script instead of supervising again, and no worker runs the chunk of this process.
int App::superviseWorkers(int count, const std::function<int()>& /*runChild*/)
{
    const HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (port == nullptr)
    {
        log::Log::error("The worker supervisor could not be started.");
        return 1;
    }

    gWorkerPort.store(port);
    SetConsoleCtrlHandler(&WinWorkerHelpers::onWorkerConsoleEvent, TRUE);
    SetEnvironmentVariableW(L"VARN_WORKER_CHILD", L"1");

    const std::wstring commandLine = GetCommandLineW();

    std::vector<WinWorkerHelpers::Worker> workers;
    workers.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        if (!WinWorkerHelpers::startWorker(commandLine, workers))
        {
            log::Log::error("Failed to spawn a worker process.");
        }
    }

    const bool started = !workers.empty();
    if (!started)
    {
        log::Log::error("No worker process could be started.");
    }

    // Restart a worker that dies abnormally, after the rest of its minimum lifetime, leave a clean exit alone, and stop on the first console control event.
    std::vector<std::chrono::steady_clock::time_point> restarts;
    bool running = started;
    while (running && (!workers.empty() || !restarts.empty()))
    {
        DWORD timeout = INFINITE;
        const auto now = std::chrono::steady_clock::now();
        if (!restarts.empty())
        {
            const auto due = *std::min_element(restarts.begin(), restarts.end());
            timeout = due > now ? static_cast<DWORD>(std::chrono::ceil<std::chrono::milliseconds>(due - now).count()) : 0;
        }

        DWORD transferred = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED overlapped = nullptr;
        if (!GetQueuedCompletionStatus(port, &transferred, &key, &overlapped, timeout))
        {
            if (GetLastError() != WAIT_TIMEOUT)
            {
                log::Log::error("The worker supervisor could not wait for its workers.");
                break;
            }

            // Restart every worker whose backoff has passed.
            const auto ready = std::chrono::steady_clock::now();
            // clang-format off
            const auto pending = std::partition(restarts.begin(), restarts.end(), [ready](auto due) { return due > ready; });
            // clang-format on
            for (auto it = pending; it != restarts.end(); ++it)
            {
                if (!WinWorkerHelpers::startWorker(commandLine, workers))
                {
                    log::Log::error("Failed to restart a worker process.");
                }
            }

            restarts.erase(pending, restarts.end());
            continue;
        }

        if (key == WinWorkerHelpers::kShutdownKey)
        {
            running = false;
            continue;
        }

        // clang-format off
        const auto exited = std::find_if(workers.begin(), workers.end(), [key](const auto& worker) { return reinterpret_cast<ULONG_PTR>(worker.process) == key; });
        // clang-format on
        if (exited == workers.end())
        {
            continue;
        }

        UnregisterWaitEx(exited->wait, INVALID_HANDLE_VALUE);
        DWORD code = 0;
        const bool known = GetExitCodeProcess(exited->process, &code) != 0;
        const auto startedAt = exited->startedAt;
        CloseHandle(exited->process);
        workers.erase(exited);

        // A worker that finished cleanly is not resurrected, and the supervisor exits once none remain.
        if (known && code == 0)
        {
            continue;
        }

        restarts.push_back(startedAt + std::chrono::milliseconds(kWorkerMinLifetimeMs));
    }

    SetConsoleCtrlHandler(&WinWorkerHelpers::onWorkerConsoleEvent, FALSE);
    for (const WinWorkerHelpers::Worker& worker : workers)
    {
        WinWorkerHelpers::stopWorker(worker);
    }

    gWorkerPort.store(nullptr);
    CloseHandle(port);
    return started ? 0 : 1;
}
#endif

bool App::isWorkerChild()
{
    const char* value = std::getenv("VARN_WORKER_CHILD");
    return value != nullptr && value[0] != '\0';
}

int App::run(int argc, char** argv)
{
    if (argc < 2)
    {
        printUsage();
        return 2;
    }

    const std::string_view first = argv[1];

    if (isVersionFlag(first))
    {
        std::printf("%s\n", VARN_VERSION_STRING);
        return 0;
    }

    if (isHelpFlag(first))
    {
        printUsage();
        return 0;
    }

    const bool eval = isEvalFlag(first);
    if (eval && argc < 3)
    {
        log::Log::error("The inline evaluation option needs a Lua source string.");
        return 2;
    }

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }

    // Set the chunk (script path or `-e` source) as `arg[0]` and arguments after it as `arg[1..]`.
    const std::size_t scriptArgIndex = eval ? 2 : 1;
    const std::string chunk = eval ? argv[2] : argv[1];

    // clang-format off
    auto runChild = [args = std::move(args), scriptArgIndex, eval, chunk]() mutable -> int
    {
        Scheduler scheduler;
        Runtime runtime(scheduler, std::move(args), scriptArgIndex);
        if (eval)
        {
            return runtime.runString(chunk.c_str(), "=(eval)");
        }

        return runtime.runScript(chunk.c_str());
    };
    // clang-format on

    // A relaunched worker child runs the script directly instead of supervising, which would otherwise recurse.
    int workers = 1;
    try
    {
        workers = workerCount();
    }
    catch (const std::invalid_argument& ex)
    {
        log::Log::error(ex.what());
        return 2;
    }

    const bool workerChild = isWorkerChild();
    if (workers > 1 && !workerChild)
    {
#if !defined(VARN_NO_FORK)
        return superviseWorkers(workers, runChild);
#else
        log::Log::line("Worker processes are not supported in this build, running a single process.");
#endif
    }

#if defined(_WIN32) && !defined(VARN_NO_FORK)
    if (workerChild)
    {
        WinWorkerHelpers::clearWorkerChildMarker();
    }
#endif

    return runChild();
}

} // namespace varn::runtime
