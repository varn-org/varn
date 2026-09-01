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
#include <utility>
#include <vector>

#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace varn::process
{

namespace
{
// bound the captured output so a child that streams without end cannot exhaust host memory
constexpr std::size_t kMaxCaptureBytes = 64 * 1024 * 1024;

enum class Drain
{
    Complete,
    Truncated,
    TimedOut
};

class PosixProcessHelpers
{
public:
    static long long remainingMs(std::chrono::steady_clock::time_point deadline)
    {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        return left > 0 ? left : 0;
    }

    // drain stdout and stderr together so a child that fills one pipe buffer while writing the other never deadlocks the parent
    static Drain drainPipes(int outFd, int errFd, std::string& outData, std::string& errData, long long timeoutMs)
    {
        std::array<char, 65536> chunk{};
        const bool bounded = timeoutMs > 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(bounded ? timeoutMs : 0);

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
            if (bounded)
            {
                wait = static_cast<int>(std::min<long long>(remainingMs(deadline), INT_MAX));
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

                break;
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

                if (got < 0 && errno == EINTR)
                {
                    continue;
                }

                // eof or a read error retires this pipe from the poll set
                fds[i].fd = -1;
            }
        }
        // clang-format on

        return Drain::Complete;
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

    // serialize pipe creation and fork so a concurrent spawn on another worker thread cannot inherit these descriptors
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
            throw std::runtime_error("[ProcessRunner] A pipe could not be created.");
        }

        // mark every pipe end close-on-exec so no other exec'd process keeps a copy that would block our reads
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
            throw std::runtime_error("[ProcessRunner] The process could not be forked.");
        }

        if (pid == 0)
        {
            // lead a new process group so killing the shell also reaches anything it forked, which still holds the inherited pipe ends
            ::setpgid(0, 0);

            // wire the write ends onto stdout and stderr, which clears their close-on-exec flag, then run the command through the shell
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

        // close the write ends inside the lock so the reads see eof once the child exits and no later spawn inherits them
        PosixProcessHelpers::closeFd(outPipe[1]);
        PosixProcessHelpers::closeFd(errPipe[1]);
    }

    ProcessResult result;
    const Drain drained = PosixProcessHelpers::drainPipes(outPipe[0], errPipe[0], result.stdoutData, result.stderrData, timeoutMs);
    PosixProcessHelpers::closeFd(outPipe[0]);
    PosixProcessHelpers::closeFd(errPipe[0]);

    // a child that overran the output cap or its deadline is killed so it cannot linger holding this thread
    if (drained != Drain::Complete)
    {
        ::kill(-pid, SIGKILL);
    }

    result.timedOut = drained == Drain::TimedOut;

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR)
    {
    }

    if (WIFEXITED(status))
    {
        result.code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        // reports a signalled exit as 128 plus the signal number
        result.code = 128 + WTERMSIG(status);
    }

    return result;
}

std::optional<std::string> ProcessRunner::getenv(const std::string& name)
{
    const char* value = ::getenv(name.c_str());
    if (value == nullptr)
    {
        return std::nullopt;
    }

    return std::string(value);
}

std::vector<std::pair<std::string, std::string>> ProcessRunner::environment()
{
    std::vector<std::pair<std::string, std::string>> entries;

    for (char** env = ::environ; env != nullptr && *env != nullptr; ++env)
    {
        const std::string line = *env;
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }

        entries.emplace_back(line.substr(0, equals), line.substr(equals + 1));
    }

    return entries;
}

std::string ProcessRunner::cwd()
{
    std::vector<char> buffer(4096);

    while (true)
    {
        if (::getcwd(buffer.data(), buffer.size()) != nullptr)
        {
            return std::string(buffer.data());
        }

        if (errno != ERANGE)
        {
            throw std::runtime_error("[ProcessRunner] The current working directory could not be read.");
        }

        buffer.resize(buffer.size() * 2);
    }
}

} // namespace varn::process
