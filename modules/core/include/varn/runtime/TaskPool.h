#pragma once

#include "varn/runtime/WorkLedger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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

#if !defined(VARN_THREADS)
    bool drainPostedJobs(std::chrono::steady_clock::time_point deadline);
    bool hasPostedJobs() const;
#endif

private:
    // The jobs that wait, in order, in a ring that grows to the longest backlog and keeps its room, so a pool that keeps posting allocates nothing once its backlog fits.
    class Queue
    {
    public:
        bool empty() const { return count == 0; }
        void push(Job job);
        Job pop();
        void dropLast();

    private:
        std::vector<Job> slots;
        std::size_t head = 0;
        std::size_t count = 0;
    };

    // The smallest ring, which a pool takes with its first job.
    static constexpr std::size_t kInitialSlots = 16;

    void runJob(Job& job);

#if defined(VARN_THREADS)
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
    Queue jobs;
    std::atomic<bool> running{true};
#if defined(VARN_THREADS)
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
