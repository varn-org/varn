#pragma once

#include "varn/runtime/EventLoop.h"
#include "varn/runtime/TaskPool.h"
#include "varn/runtime/WorkLedger.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>

namespace varn::runtime
{

class Scheduler
{
public:
    struct Options
    {
        std::size_t taskThreads;
        std::size_t ioThreads;
    };

    explicit Scheduler(Options options = {});
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    bool poll();
    EventLoop::PollResult poll(std::chrono::nanoseconds budget);
    void stop();
    bool stopped() const { return stopFlag.load(std::memory_order_acquire); }
    bool hasWorkInFlight() const;

    EventLoop& mainLoop();
    TaskPool& taskPool();
    TaskPool& ioPool();

private:
    std::shared_ptr<WorkLedger> workLedger;
    EventLoop loop;
    TaskPool taskWorkers;
    TaskPool ioWorkers;
    std::atomic<bool> stopFlag{false};

#if !defined(VARN_THREADS)
    bool drainPools(std::chrono::steady_clock::time_point deadline);
    bool poolsHaveJobs();
#endif
};

} // namespace varn::runtime
