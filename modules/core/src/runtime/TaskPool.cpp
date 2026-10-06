#include "varn/runtime/TaskPool.h"

#include <stdexcept>

namespace varn::runtime
{

#if !defined(__EMSCRIPTEN__)
namespace
{
// The pool whose worker runs on this thread, so a stop from inside a job knows not to wait for its own thread.
thread_local const TaskPool* workerPool = nullptr;
} // namespace
#endif

// A pool starts its threads only as its jobs need them, up to its limit, and a thread idle for the keep-alive retires, while the browser runs every job on the pump and keeps no thread.
TaskPool::TaskPool(std::size_t threadLimit, [[maybe_unused]] std::chrono::milliseconds keepAlive, std::shared_ptr<WorkLedger> ledger)
    : ledger(std::move(ledger))
    , limit(threadLimit)
#if !defined(__EMSCRIPTEN__)
    , keepAlive(keepAlive)
#endif
{
    if (limit == 0)
    {
        throw std::invalid_argument("A worker pool needs a limit of at least one thread.");
    }
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
            jobs.push_back([ledger = ledger, j = std::move(job)]() mutable
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
        if (!wakeWorker && shouldGrow())
        {
            try
            {
                grow();
            }
            catch (...)
            {
                // Without a live worker the job could never run, so the post fails as a whole.
                if (liveWorkers == 0)
                {
                    jobs.pop_back();
                    ledger->leave();
                    throw;
                }
            }
        }
#endif
    }

#if !defined(__EMSCRIPTEN__)
    if (wakeWorker)
    {
        cv.notify_one();
    }
#endif
}

std::size_t TaskPool::threadCount() const
{
#if defined(__EMSCRIPTEN__)
    return 0;
#else
    std::lock_guard<std::mutex> lock(mutex);
    return liveWorkers;
#endif
}

std::size_t TaskPool::idleThreadCount() const
{
#if defined(__EMSCRIPTEN__)
    return 0;
#else
    std::lock_guard<std::mutex> lock(mutex);
    return idleWorkers;
#endif
}

#if defined(__EMSCRIPTEN__)
// Runs the posted jobs in order until none is left or the deadline passed, always at least one, and answers whether any ran.
bool TaskPool::drainPostedJobs(std::chrono::steady_clock::time_point deadline)
{
    bool ran = false;
    for (;;)
    {
        Job job;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (jobs.empty())
            {
                return ran;
            }

            job = std::move(jobs.front());
            jobs.pop_front();
        }

        job();
        ran = true;
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return ran;
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
        running = false;
    }

    cv.notify_all();

    // Every caller waits for every other worker, so a destructor after a stop from another thread never frees a pool in use, and a worker that stops its own pool leaves its thread to the stop that follows.
    const auto self = std::this_thread::get_id();
    const std::size_t remaining = workerPool == this ? 1 : 0;
    for (;;)
    {
        std::list<std::thread> threads;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto& worker : workers)
            {
                if (worker.joinable() && worker.get_id() != self)
                {
                    threads.push_back(std::move(worker));
                }
            }

            if (lastRetired.joinable())
            {
                threads.push_back(std::move(lastRetired));
            }

            if (threads.empty() && liveWorkers == remaining)
            {
                return;
            }
        }

        for (auto& thread : threads)
        {
            thread.join();
        }
    }
#endif
}

#if !defined(__EMSCRIPTEN__)
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

// A starting worker takes a job and starts the next one while jobs still wait, so a burst grows the pool one thread at a time instead of one per post.
bool TaskPool::shouldGrow() const
{
    return running && !jobs.empty() && idleWorkers == 0 && startingWorkers == 0 && liveWorkers < limit;
}

// The new worker takes the mutex held here before anything else, so it always finds its own thread in place.
void TaskPool::grow()
{
    workers.emplace_back();
    const Worker self = std::prev(workers.end());
    try
    {
        // clang-format off
        *self = std::thread([this, self]
        {
            workerLoop(self);
        });
        // clang-format on
    }
    catch (...)
    {
        workers.erase(self);
        throw;
    }

    ++liveWorkers;
    ++startingWorkers;
}

void TaskPool::workerLoop(Worker self)
{
    workerPool = this;
    std::unique_lock<std::mutex> lock(mutex);
    --startingWorkers;

    for (;;)
    {
        if (running && jobs.empty())
        {
            ++idleWorkers;
            // clang-format off
            const bool woken = cv.wait_for(lock, keepAlive, [this]
            {
                return !running || !jobs.empty();
            });
            // clang-format on
            --idleWorkers;

            // A wait that timed out found no job, so no signal was on its way to this worker.
            if (!woken)
            {
                break;
            }
        }

        if (!running)
        {
            break;
        }

        Job job = std::move(jobs.front());
        jobs.pop_front();
        wakePending = false;

        const bool wakeNext = claimWake();
        if (!wakeNext && shouldGrow())
        {
            try
            {
                grow();
            }
            catch (...)
            {
                // This worker runs the waiting jobs once it frees.
            }
        }

        lock.unlock();
        if (wakeNext)
        {
            cv.notify_one();
        }

        job();
        job = nullptr;
        lock.lock();
    }

    std::thread previous = retire(self);
    lock.unlock();

    if (previous.joinable())
    {
        previous.join();
    }
}

// A thread cannot join itself, so each retiring worker parks its own thread and joins the one parked before it.
std::thread TaskPool::retire(Worker self)
{
    std::thread previous = std::move(lastRetired);
    lastRetired = std::move(*self);
    workers.erase(self);
    --liveWorkers;
    return previous;
}
#endif

} // namespace varn::runtime
