#include "varn/process/ProcessRunner.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace varn::process
{

namespace
{
// Bound the captured output so a child that streams without end cannot exhaust host memory.
constexpr std::size_t kMaxCaptureBytes = 64 * 1024 * 1024;

enum class Drain
{
    Complete,
    Truncated,
    TimedOut,
    Failed
};

class PosixProcessHelpers
{
public:
    static long long remainingMs(std::chrono::steady_clock::time_point deadline)
    {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        return left > 0 ? left : 0;
    }

    // Drain stdout and stderr together so a child that fills one pipe buffer while writing the other never deadlocks the parent.
    static Drain drainPipes(int outFd, int errFd, std::string& outData, std::string& errData, std::optional<std::chrono::steady_clock::time_point> deadline)
    {
        std::array<char, 65536> chunk{};

        // clang-format off
        pollfd fds[2];
        fds[0].fd = outFd;
        fds[1].fd = errFd;

        while (fds[0].fd >= 0 || fds[1].fd >= 0)
        {
            fds[0].events = fds[0].fd >= 0 ? POLLIN : 0;
            fds[1].events = fds[1].fd >= 0 ? POLLIN : 0;
            fds[0].revents = 0;
            fds[1].revents = 0;

            int wait = -1;
            if (deadline)
            {
                wait = static_cast<int>(std::min<long long>(remainingMs(*deadline), INT_MAX));
                if (wait == 0)
                {
                    return Drain::TimedOut;
                }
            }

            const int ready = ::poll(fds, 2, wait);
            if (ready == 0)
            {
                return Drain::TimedOut;
            }

            if (ready < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                return Drain::Failed;
            }

            for (int i = 0; i < 2; ++i)
            {
                if (fds[i].fd < 0 || fds[i].revents == 0)
                {
                    continue;
                }

                const ssize_t got = ::read(fds[i].fd, chunk.data(), chunk.size());
                if (got > 0)
                {
                    std::string& sink = (i == 0 ? outData : errData);
                    const std::size_t captured = outData.size() + errData.size();
                    const std::size_t room = captured < kMaxCaptureBytes ? kMaxCaptureBytes - captured : 0;
                    sink.append(chunk.data(), std::min(room, static_cast<std::size_t>(got)));

                    if (static_cast<std::size_t>(got) >= room)
                    {
                        return Drain::Truncated;
                    }

                    continue;
                }

                if (got < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }

                    return Drain::Failed;
                }

                // The end of the stream retires this pipe from the poll set.
                fds[i].fd = -1;
            }
        }
        // clang-format on

        return Drain::Complete;
    }

    // Waits for the child to exit until the deadline, polling with a delay that grows, since POSIX offers no portable wait with a timeout for a child.
    static std::optional<int> waitUntil(pid_t pid, std::chrono::steady_clock::time_point deadline)
    {
        std::chrono::milliseconds pause(1);
        while (true)
        {
            int status = 0;
            const pid_t reaped = ::waitpid(pid, &status, WNOHANG);
            if (reaped == pid)
            {
                return status;
            }

            if (reaped < 0 && errno != EINTR)
            {
                throw std::runtime_error("The exit status of the command could not be read: " + std::string(std::strerror(errno)) + ".");
            }

            const long long left = remainingMs(deadline);
            if (left == 0)
            {
                return std::nullopt;
            }

            std::this_thread::sleep_for(std::min(pause, std::chrono::milliseconds(left)));
            pause = std::min(pause * 2, std::chrono::milliseconds(50));
        }
    }

    static int waitForExit(pid_t pid)
    {
        int status = 0;
        while (::waitpid(pid, &status, 0) < 0)
        {
            if (errno != EINTR)
            {
                throw std::runtime_error("The exit status of the command could not be read: " + std::string(std::strerror(errno)) + ".");
            }
        }

        return status;
    }

    static void closeFd(int& fd)
    {
        if (fd >= 0)
        {
            ::close(fd);
            fd = -1;
        }
    }

    static void setCloexec(int fd)
    {
        const int flags = ::fcntl(fd, F_GETFD);
        if (flags >= 0)
        {
            ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
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
    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};

    // Serialize pipe creation and fork so a concurrent spawn on another worker thread cannot inherit these descriptors.
    static std::mutex spawnMutex;
    pid_t pid = -1;
    {
        std::lock_guard<std::mutex> lock(spawnMutex);

        if (::pipe(outPipe) != 0 || ::pipe(errPipe) != 0)
        {
            PosixProcessHelpers::closeFd(outPipe[0]);
            PosixProcessHelpers::closeFd(outPipe[1]);
            PosixProcessHelpers::closeFd(errPipe[0]);
            PosixProcessHelpers::closeFd(errPipe[1]);
            throw std::runtime_error("A pipe could not be created.");
        }

        // Mark every pipe end close-on-exec so no other exec'd process keeps a copy that would block our reads.
        PosixProcessHelpers::setCloexec(outPipe[0]);
        PosixProcessHelpers::setCloexec(outPipe[1]);
        PosixProcessHelpers::setCloexec(errPipe[0]);
        PosixProcessHelpers::setCloexec(errPipe[1]);

        pid = ::fork();
        if (pid < 0)
        {
            PosixProcessHelpers::closeFd(outPipe[0]);
            PosixProcessHelpers::closeFd(outPipe[1]);
            PosixProcessHelpers::closeFd(errPipe[0]);
            PosixProcessHelpers::closeFd(errPipe[1]);
            throw std::runtime_error("The process could not be forked.");
        }

        if (pid == 0)
        {
            // Lead a new process group so killing the shell also reaches anything it forked, which still holds the inherited pipe ends.
            ::setpgid(0, 0);

            // Wire the write ends onto stdout and stderr, which clears their close-on-exec flag, then run the command through the shell.
            ::dup2(outPipe[1], STDOUT_FILENO);
            ::dup2(errPipe[1], STDERR_FILENO);
            ::close(outPipe[0]);
            ::close(outPipe[1]);
            ::close(errPipe[0]);
            ::close(errPipe[1]);

#if defined(__ANDROID__)
            ::execl("/system/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
#else
            ::execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
#endif
            ::_exit(127);
        }

        // The parent sets the group too, so a kill that comes before the child ran its own call still reaches the group, and whichever call runs second finds it already set.
        ::setpgid(pid, pid);

        // Close the write ends inside the lock so the reads see EOF once the child exits and no later spawn inherits them.
        PosixProcessHelpers::closeFd(outPipe[1]);
        PosixProcessHelpers::closeFd(errPipe[1]);
    }

    // One deadline covers the output and the exit, so a child that closes its streams and keeps running is still killed when it passes.
    std::optional<std::chrono::steady_clock::time_point> deadline;
    if (timeoutMs > 0)
    {
        deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    }

    ProcessResult result;
    Drain drained = PosixProcessHelpers::drainPipes(outPipe[0], errPipe[0], result.stdoutData, result.stderrData, deadline);
    PosixProcessHelpers::closeFd(outPipe[0]);
    PosixProcessHelpers::closeFd(errPipe[0]);

    std::optional<int> status;
    if (drained == Drain::Complete && deadline)
    {
        status = PosixProcessHelpers::waitUntil(pid, *deadline);
        if (!status)
        {
            drained = Drain::TimedOut;
        }
    }

    // A child that overran the output cap or its deadline, or whose output could not be read, is killed so it cannot linger holding this thread.
    if (drained != Drain::Complete)
    {
        ::kill(-pid, SIGKILL);
    }

    if (!status)
    {
        status = PosixProcessHelpers::waitForExit(pid);
    }

    if (drained == Drain::Failed)
    {
        throw std::runtime_error("The output of the command could not be read.");
    }

    result.timedOut = drained == Drain::TimedOut;

    if (WIFEXITED(*status))
    {
        result.code = WEXITSTATUS(*status);
    }
    else if (WIFSIGNALED(*status))
    {
        // Reports a signalled exit as 128 plus the signal number.
        result.code = 128 + WTERMSIG(*status);
    }

    return result;
}

} // namespace varn::process
