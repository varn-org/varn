#pragma once

#include "varn/runtime/WorkLedger.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace varn::runtime
{

class TaskPool
{
public:
    using Job = std::function<void()>;

    explicit TaskPool(std::size_t threadCount, std::shared_ptr<WorkLedger> ledger);
    ~TaskPool();

    TaskPool(const TaskPool&) = delete;
    TaskPool& operator=(const TaskPool&) = delete;

    void start();
    void post(Job job);
    void stop();

#if defined(__EMSCRIPTEN__)
    void drainPostedJobs();
    bool hasPostedJobs() const;
#endif

private:
    void workerLoop();

    std::shared_ptr<WorkLedger> ledger;
    std::size_t threadCount;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::queue<Job> jobs;
    std::vector<std::thread> workers;
    std::atomic<bool> running{true};
};

} // namespace varn::runtime
