#pragma once

#include "varn/runtime/WorkLedger.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#if !defined(__EMSCRIPTEN__)
namespace Poco
{
namespace Net
{
class Socket;
}
} // namespace Poco
#endif

namespace varn::runtime
{

class EventLoop
{
public:
    using Job = std::function<void()>;
    using IdleExitPredicate = std::function<bool()>;
    using IoHandler = std::function<bool()>;

    explicit EventLoop(std::shared_ptr<WorkLedger> ledger);
    ~EventLoop();

    void post(Job job);
    void postDelayed(long long delayMs, Job job);
    void run();
    void stop();
    void wake();

    void clearPendingJobs();

    void setIdleExitPredicate(IdleExitPredicate predicate);

    bool hasPendingJobs() const;
    bool hasPendingTimers() const;

#if !defined(__EMSCRIPTEN__)
    void watchRead(const Poco::Net::Socket& socket, IoHandler handler);
    void watchWrite(const Poco::Net::Socket& socket, IoHandler handler);
    void closeSocket(const Poco::Net::Socket& socket);
    bool isRunning() const;
    void shutdownIo();
#endif

#if defined(__EMSCRIPTEN__)
    void drainPostedJobs();
#endif

private:
    struct Poller;
    void wakeFromAnotherThread();
#if !defined(__EMSCRIPTEN__)
    bool onLoopThread() const;
#endif

    std::shared_ptr<WorkLedger> ledger;
    mutable std::mutex mutex;
    std::queue<Job> jobs;
    std::multimap<std::chrono::steady_clock::time_point, Job> timers;
    std::atomic<bool> running{false};
    IdleExitPredicate idleExitEligible;
    std::atomic<std::thread::id> loopThread{};
    std::unique_ptr<Poller> poller;
};

} // namespace varn::runtime
