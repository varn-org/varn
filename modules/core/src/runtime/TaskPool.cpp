#include "varn/runtime/TaskPool.h"

namespace varn::runtime
{

// The browser runs every job on the pump, so a pool there keeps no count of the threads it never starts.
TaskPool::TaskPool([[maybe_unused]] std::size_t threadCount, std::shared_ptr<WorkLedger> ledger)
    : ledger(std::move(ledger))
#if !defined(__EMSCRIPTEN__)
    , threadCount(threadCount == 0 ? 4 : threadCount)
#endif
{
}

void TaskPool::start()
{
    // Spawn workers only after the ledger notify hook is installed so none observes a half-set callback.
#if !defined(__EMSCRIPTEN__)
    if (!workers.empty())
    {
        return;
    }

    workers.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i)
    {
        workers.emplace_back([this]
                             { workerLoop(); });
    }
#endif
}

TaskPool::~TaskPool()
{
#if !defined(__EMSCRIPTEN__)
    stop();
#endif
}

void TaskPool::post(Job job)
{
#if !defined(__EMSCRIPTEN__)
    bool wakeWorker = false;
#endif
    {
        std::lock_guard<std::mutex> lock(mutex);

        // Drop work once stopped so a job no worker will ever run cannot leak a ledger entry.
        if (!running)
        {
            return;
        }

        ledger->enter();
        // clang-format off
        try
        {
            jobs.push([ledger = ledger, j = std::move(job)]() mutable
            {
                // Release the ledger entry even if the job throws so the pool keeps draining.
                try
                {
                    j();
                }
                catch (...)
                {
                }

                ledger->leave();
            });
        }
        catch (...)
        {
            // The job never entered the queue, so release the entry it will never run to balance.
            ledger->leave();
            throw;
        }
        // clang-format on

#if !defined(__EMSCRIPTEN__)
        wakeWorker = claimWake();
#endif
    }

#if !defined(__EMSCRIPTEN__)
    if (wakeWorker)
    {
        cv.notify_one();
    }
#endif
}

#if defined(__EMSCRIPTEN__)
void TaskPool::drainPostedJobs()
{
    for (;;)
    {
        Job job;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (jobs.empty())
            {
                break;
            }

            job = std::move(jobs.front());
            jobs.pop();
        }

        if (job)
        {
            job();
        }
    }
}

bool TaskPool::hasPostedJobs() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return !jobs.empty();
}
#endif

void TaskPool::stop()
{
#if defined(__EMSCRIPTEN__)
    running = false;
#else
    {
        // Change the flag under the mutex the workers wait on so a worker that has evaluated the predicate but not yet blocked does not miss the notify.
        std::lock_guard<std::mutex> lock(mutex);
        bool expected = true;
        if (!running.compare_exchange_strong(expected, false))
        {
            return;
        }
    }

    cv.notify_all();

    for (auto& worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
#endif
}

// Answers whether a worker should be signalled, which is only while a job waits, a worker is idle and no signal is already on its way to one, and is called with the mutex held.
// A busy worker takes the next job by itself before it waits again, so a burst of posts signals one worker and each worker that takes a job passes the signal on while jobs remain.
bool TaskPool::claimWake()
{
    if (jobs.empty() || idleWorkers == 0 || wakePending)
    {
        return false;
    }

    wakePending = true;
    return true;
}

void TaskPool::workerLoop()
{
    while (running)
    {
        Job job;
        bool wakeNext = false;

        {
            std::unique_lock<std::mutex> lock(mutex);
            ++idleWorkers;
            cv.wait(lock, [&]
                    { return !running || !jobs.empty(); });
            --idleWorkers;
            wakePending = false;

            if (!running && jobs.empty())
            {
                break;
            }

            job = std::move(jobs.front());
            jobs.pop();
            wakeNext = claimWake();
        }

        if (wakeNext)
        {
            cv.notify_one();
        }

        if (job)
        {
            job();
        }
    }
}

} // namespace varn::runtime
