#include "varn/runtime/TaskPool.h"

namespace varn::runtime
{

TaskPool::TaskPool(std::size_t threadCount, std::shared_ptr<WorkLedger> ledger)
    : ledger(std::move(ledger))
    , threadCount(threadCount == 0 ? 4 : threadCount)
{
}

void TaskPool::start()
{
    // spawn workers only after the ledger notify hook is installed so none observes a half-set callback
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
    {
        std::lock_guard<std::mutex> lock(mutex);

        // drop work once stopped so a job no worker will ever run cannot leak a ledger entry
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
                // release the ledger entry even if the job throws so the pool keeps draining
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
            // the job never entered the queue, so release the entry it will never run to balance
            ledger->leave();
            throw;
        }
        // clang-format on
    }
#if !defined(__EMSCRIPTEN__)
    cv.notify_one();
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
        // change the flag under the mutex the workers wait on so a worker that has evaluated the predicate but not yet blocked does not miss the notify
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

void TaskPool::workerLoop()
{
    while (running)
    {
        Job job;

        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&]
                    { return !running || !jobs.empty(); });

            if (!running && jobs.empty())
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

} // namespace varn::runtime
