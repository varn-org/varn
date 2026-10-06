#pragma once

#include "varn/runtime/WorkLedger.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

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
    using WakeHandler = std::function<void()>;
    using TimerId = std::uint64_t;

    struct PollResult
    {
        bool ran = false;
        bool pending = false;
        long long idleMilliseconds = -1;
    };

    explicit EventLoop(std::shared_ptr<WorkLedger> ledger);
    ~EventLoop();

    void post(Job job);
    TimerId postDelayed(long long delayMs, Job job);
    bool cancelTimer(TimerId id);
    void enter();
    void run();
    bool poll();
    PollResult poll(std::chrono::nanoseconds budget);
    void stop();
    void wake();

    void clearPendingJobs();

    void retain();
    bool release();

    void setIdleExitPredicate(IdleExitPredicate predicate);
    void setWakeHandler(WakeHandler handler);
    long long idleMilliseconds() const;

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

    struct PumpScope
    {
        explicit PumpScope(EventLoop& loop);
        ~PumpScope();
        PumpScope(const PumpScope&) = delete;
        PumpScope& operator=(const PumpScope&) = delete;

        EventLoop& loop;
    };

    struct Timer
    {
        TimerId id;
        Job job;
    };

    using Timers = std::multimap<std::chrono::steady_clock::time_point, Timer>;

    using Clock = std::chrono::steady_clock;

    void queueDueTimers();
    std::size_t runPass(Clock::time_point deadline);
    std::size_t pollIo();
    bool pending() const;
    void wakeFromAnotherThread();
#if !defined(__EMSCRIPTEN__)
    bool onLoopThread() const;
#endif

    std::shared_ptr<WorkLedger> ledger;
    mutable std::mutex mutex;
    std::queue<Job> jobs;
    Timers timers;
    std::unordered_map<TimerId, Timers::iterator> timerIndex;
    TimerId nextTimer = 0;
    std::atomic<bool> running{false};
    int keepAlive = 0;
    IdleExitPredicate idleExitEligible;
    std::atomic<std::thread::id> loopThread{};
    std::unique_ptr<Poller> poller;
    mutable std::mutex wakeMutex;
    WakeHandler wakeHandler;
    bool pumping = false;
};

} // namespace varn::runtime
