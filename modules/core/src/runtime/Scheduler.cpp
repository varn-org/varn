#include "varn/runtime/Scheduler.h"
#include "varn/runtime/HostWork.h"

#include <thread>

namespace varn::runtime
{

namespace
{
// The threads of the I/O pool mostly wait on files, processes and transfers, so it may run many more of them than there are cores.
constexpr std::size_t kIoThreads = 512;

constexpr std::size_t kTaskThreadsWithoutCoreCount = 4;

constexpr std::chrono::milliseconds kWorkerKeepAlive{10000};

class SchedulerHelpers
{
public:
    // The standard answers zero when it cannot count the cores.
    static std::size_t coreCount()
    {
        const unsigned cores = std::thread::hardware_concurrency();
        return cores != 0 ? cores : kTaskThreadsWithoutCoreCount;
    }
};
} // namespace

// A pool the options leave at zero takes its default limit, a thread per core for the tasks and a high count for the blocking I/O.
Scheduler::Scheduler(Options options)
    : workLedger(std::make_shared<WorkLedger>())
    , loop(workLedger)
    , taskWorkers(options.taskThreads != 0 ? options.taskThreads : SchedulerHelpers::coreCount(), kWorkerKeepAlive, workLedger)
    , ioWorkers(options.ioThreads != 0 ? options.ioThreads : kIoThreads, kWorkerKeepAlive, workLedger)
{
    // No pool starts a worker before its first job, which nothing posts before the hook is installed here.
    // The ledger notifies only when its last entry leaves, which is the one change that can let an idle loop exit.
    workLedger->setNotify([this]
                          { loop.wake(); });
}

Scheduler::~Scheduler()
{
    stop();
}

// Advances the loop without blocking, running every pass of work there is, and answers whether anything can still make progress.
bool Scheduler::poll()
{
    if (stopped())
    {
        return false;
    }

#if defined(VARN_THREADS)
    const bool more = loop.poll();
#else
    // A build without threads runs a pool job only when the pump drains it, so the pools and the loop take turns until neither makes progress.
    bool more = false;
    for (bool progressed = true; progressed;)
    {
        const bool drained = drainPools(std::chrono::steady_clock::time_point::max());
        const EventLoop::PollResult step = loop.poll(std::chrono::nanoseconds::max());
        progressed = drained || step.ran;
        more = step.pending;
    }
#endif

    // The work of the host, such as a fetch of the browser, completes through a callback of the host rather than through the queue, so it still expects a later poll.
    return more || HostWork::pending();
}

// Advances the loop without blocking for about as long as the budget allows, and answers whether anything ran, whether anything can still make progress and how long the host can sleep.
EventLoop::PollResult Scheduler::poll(std::chrono::nanoseconds budget)
{
    if (stopped())
    {
        return {};
    }

#if defined(VARN_THREADS)
    EventLoop::PollResult result = loop.poll(budget);
#else
    // A build without threads drains the pools before every poll of the loop and polls the loop again while either one makes progress.
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = EventLoop::deadlineAfter(started, budget);
    EventLoop::PollResult result;
    for (;;)
    {
        const bool drained = drainPools(deadline);
        const auto spent = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started);
        const EventLoop::PollResult step = loop.poll(budget > spent ? budget - spent : std::chrono::nanoseconds::zero());
        result.ran = result.ran || drained || step.ran;
        result.pending = step.pending;
        result.idleMilliseconds = step.idleMilliseconds;
        if ((!drained && !step.ran) || std::chrono::steady_clock::now() >= deadline)
        {
            break;
        }
    }

    // A job a pass posted to a pool runs in the next poll, so the host polls again at once.
    if (poolsHaveJobs())
    {
        result.pending = true;
        result.idleMilliseconds = 0;
    }
#endif

    // The work of the host, such as a fetch of the browser, completes through a callback of the host rather than through the queue, so it still expects a later poll.
    result.pending = result.pending || HostWork::pending();
    return result;
}

#if !defined(VARN_THREADS)
// Runs pool jobs until none is left or the deadline passed, answering whether any ran.
bool Scheduler::drainPools(std::chrono::steady_clock::time_point deadline)
{
    const bool tasks = taskWorkers.drainPostedJobs(deadline);
    const bool io = ioWorkers.drainPostedJobs(deadline);
    return tasks || io;
}

bool Scheduler::poolsHaveJobs()
{
    return taskWorkers.hasPostedJobs() || ioWorkers.hasPostedJobs();
}
#endif

// Stops the pools, waiting for the jobs that run, stops the loop and drops every job and timer still queued without running it, from any thread.
void Scheduler::stop()
{
    bool expected = false;
    if (!stopFlag.compare_exchange_strong(expected, true))
    {
        return;
    }

    taskWorkers.stop();
    ioWorkers.stop();

    loop.stop();
    loop.clearPendingJobs();
}

// Answers whether a job of the loop or of a pool, or a timer, has not finished yet.
bool Scheduler::hasWorkInFlight() const
{
    return workLedger->depth() != 0;
}

EventLoop& Scheduler::mainLoop()
{
    return loop;
}

TaskPool& Scheduler::taskPool()
{
    return taskWorkers;
}

TaskPool& Scheduler::ioPool()
{
    return ioWorkers;
}

} // namespace varn::runtime
