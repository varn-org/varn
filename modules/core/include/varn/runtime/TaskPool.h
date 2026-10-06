#pragma once

#include "varn/runtime/WorkLedger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

namespace varn::runtime
{

class TaskPool
{
public:
    using Job = std::function<void()>;

    TaskPool(std::size_t threadLimit, std::chrono::milliseconds keepAlive, std::shared_ptr<WorkLedger> ledger);
    ~TaskPool();

    TaskPool(const TaskPool&) = delete;
    TaskPool& operator=(const TaskPool&) = delete;

    void post(Job job);
    void stop();
    std::size_t threadCount() const;
    std::size_t idleThreadCount() const;
    std::size_t threadLimit() const { return limit; }

#if defined(__EMSCRIPTEN__)
    bool drainPostedJobs(std::chrono::steady_clock::time_point deadline);
    bool hasPostedJobs() const;
#endif

private:
#if !defined(__EMSCRIPTEN__)
    using Worker = std::list<std::thread>::iterator;

    bool claimWake();
    bool shouldGrow() const;
    void grow();
    void workerLoop(Worker self);
    std::thread retire(Worker self);
#endif

    std::shared_ptr<WorkLedger> ledger;
    std::size_t limit;
    mutable std::mutex mutex;
    std::deque<Job> jobs;
    std::atomic<bool> running{true};
#if !defined(__EMSCRIPTEN__)
    std::chrono::milliseconds keepAlive;
    std::size_t liveWorkers = 0;
    std::size_t startingWorkers = 0;
    std::size_t idleWorkers = 0;
    bool wakePending = false;
    std::condition_variable cv;
    std::list<std::thread> workers;
    std::thread lastRetired;
#endif
};

} // namespace varn::runtime
