#include "varn/runtime/EventLoop.h"

#if !defined(__EMSCRIPTEN__)
#include <Poco/Net/Socket.h>
#include <atomic>
#include <deque>
#include <uv.h>
#include <vector>
#endif

#include <algorithm>

namespace varn::runtime
{

#if !defined(__EMSCRIPTEN__)

struct EventLoop::Poller
{
    struct SocketState
    {
        uv_poll_t handle;
        Poco::Net::Socket socket;
        std::deque<IoHandler> readers;
        std::deque<IoHandler> writers;
        Poller* owner = nullptr;
        bool retiring = false;
    };

    uv_loop_t loop;
    uv_async_t async;
    uv_timer_t timer;
    std::map<Poco::Net::Socket, SocketState*> entries;
    std::mutex commandMutex;
    std::vector<std::function<void()>> commands;
    std::atomic<bool> closed{false};
    std::size_t handlerRuns = 0;
    std::size_t closing = 0;

    // Bounds the handlers one direction of a socket runs per readiness, so a peer that keeps a socket ready cannot hold the loop.
    static constexpr std::size_t kServeBatch = 256;

    Poller()
    {
        uv_loop_init(&loop);
        uv_async_init(&loop, &async, nullptr);
        uv_timer_init(&loop, &timer);
        async.data = this;
        timer.data = this;
    }

    ~Poller()
    {
        if (!closed)
        {
            clear();
        }
    }

    static void onPoll(uv_poll_t* handle, int status, int events)
    {
        static_cast<SocketState*>(handle->data)->owner->serve(static_cast<SocketState*>(handle->data), status, events);
    }

    static void onClosed(uv_handle_t* handle)
    {
        auto* state = static_cast<SocketState*>(handle->data);
        --state->owner->closing;
        delete state;
    }

    // Answers the state that watches the socket, or null when libuv cannot watch it, such as a socket already closed.
    SocketState* ensure(const Poco::Net::Socket& socket)
    {
        auto it = entries.find(socket);
        if (it != entries.end())
        {
            return it->second;
        }

        auto state = std::make_unique<SocketState>();
        state->socket = socket;
        state->owner = this;
        state->handle.data = state.get();
        if (uv_poll_init_socket(&loop, &state->handle, socket.impl()->sockfd()) != 0)
        {
            return nullptr;
        }

        entries.emplace(socket, state.get());
        return state.release();
    }

    // A handler of a socket that cannot be watched runs once, the way a close runs the pending ones, so its operation settles instead of waiting forever.
    void addReader(const Poco::Net::Socket& socket, IoHandler handler)
    {
        SocketState* state = ensure(socket);
        if (state == nullptr)
        {
            runOnce(handler);
            return;
        }

        state->readers.push_back(std::move(handler));
        refreshMode(state);
    }

    void addWriter(const Poco::Net::Socket& socket, IoHandler handler)
    {
        SocketState* state = ensure(socket);
        if (state == nullptr)
        {
            runOnce(handler);
            return;
        }

        state->writers.push_back(std::move(handler));
        refreshMode(state);
    }

    static void runOnce(const IoHandler& handler)
    {
        try
        {
            handler();
        }
        catch (...)
        {
        }
    }

    void refreshMode(SocketState* state)
    {
        if (state->retiring)
        {
            return;
        }

        int mask = 0;
        if (!state->readers.empty())
        {
            mask |= UV_READABLE;
        }

        if (!state->writers.empty())
        {
            mask |= UV_WRITABLE;
        }

        if (mask == 0)
        {
            retire(state);
            return;
        }

        uv_poll_start(&state->handle, mask, onPoll);
    }

    void retire(SocketState* state)
    {
        if (state->retiring)
        {
            return;
        }

        // Mark the state retiring so it outlives this call until libuv frees the handle in its close callback.
        state->retiring = true;
        entries.erase(state->socket);
        uv_poll_stop(&state->handle);
        ++closing;
        uv_close(reinterpret_cast<uv_handle_t*>(&state->handle), &Poller::onClosed);
    }

    void serve(SocketState* state, int status, int events)
    {
        const bool errored = status < 0;

        if (((events & UV_READABLE) || errored) && !serveQueue(state, state->readers))
        {
            return;
        }

        if (((events & UV_WRITABLE) || errored) && !serveQueue(state, state->writers))
        {
            return;
        }

        refreshMode(state);
    }

    // Runs the handlers of one direction in order while each one completes, so a ready socket serves every waiting read or write in one readiness, and answers false once the socket retired under them.
    bool serveQueue(SocketState* state, std::deque<IoHandler>& handlers)
    {
        for (std::size_t served = 0; served < kServeBatch && !handlers.empty(); ++served)
        {
            // Pop the handler before invoking it so a close triggered inside the handler cannot rerun it.
            IoHandler handler = std::move(handlers.front());
            handlers.pop_front();
            bool done = true;
            try
            {
                done = handler();
            }
            catch (...)
            {
            }

            ++handlerRuns;
            if (state->retiring)
            {
                return false;
            }

            if (!done)
            {
                handlers.push_front(std::move(handler));
                return true;
            }
        }

        return true;
    }

    void closeNow(const Poco::Net::Socket& socket)
    {
        auto it = entries.find(socket);
        if (it == entries.end())
        {
            try
            {
                socket.impl()->close();
            }
            catch (...)
            {
            }

            return;
        }

        SocketState* state = it->second;
        std::deque<IoHandler> readers = std::move(state->readers);
        std::deque<IoHandler> writers = std::move(state->writers);

        // Stop watching the descriptor before it closes, since another thread may reuse its number at once and the poller must never touch that one.
        // The closed descriptor then fails the next I/O call of each pending handler, which runs so its promise rejects.
        retire(state);
        try
        {
            state->socket.impl()->close();
        }
        catch (...)
        {
        }

        for (auto& reader : readers)
        {
            runOnce(reader);
        }

        for (auto& writer : writers)
        {
            runOnce(writer);
        }
    }

    void wakeAsync()
    {
        // Serialize the wakeup with clear so a cross-thread send never races the handle being closed.
        std::lock_guard<std::mutex> lock(commandMutex);
        if (!closed)
        {
            uv_async_send(&async);
        }
    }

    void submit(std::function<void()> command)
    {
        std::lock_guard<std::mutex> lock(commandMutex);
        if (closed)
        {
            return;
        }

        commands.push_back(std::move(command));
        uv_async_send(&async);
    }

    std::size_t drainCommands()
    {
        std::vector<std::function<void()>> pending;
        {
            std::lock_guard<std::mutex> lock(commandMutex);
            pending.swap(commands);
        }

        for (auto& command : pending)
        {
            try
            {
                command();
            }
            catch (...)
            {
            }
        }

        return pending.size();
    }

    bool hasSockets() const
    {
        return !entries.empty();
    }

    // Answers whether a pass of libuv can do anything, which is only while a socket is watched or a closed one waits for its close callback.
    // An empty poll still costs the system a call, and on some systems a call that waits far longer than it takes to return a ready event.
    bool hasHandles() const
    {
        return !entries.empty() || closing > 0;
    }

    void clear()
    {
        {
            // Mark closed under the same mutex the wakeups use so no cross-thread send can target the async handle after this.
            std::lock_guard<std::mutex> lock(commandMutex);
            if (closed)
            {
                return;
            }

            closed = true;
            commands.clear();
        }

        std::vector<SocketState*> all;
        all.reserve(entries.size());
        for (auto& entry : entries)
        {
            all.push_back(entry.second);
        }

        entries.clear();
        for (auto* state : all)
        {
            state->retiring = true;
            uv_poll_stop(&state->handle);
            ++closing;
            uv_close(reinterpret_cast<uv_handle_t*>(&state->handle), &Poller::onClosed);
        }

        uv_close(reinterpret_cast<uv_handle_t*>(&async), nullptr);
        uv_close(reinterpret_cast<uv_handle_t*>(&timer), nullptr);
        // Run the loop until every close callback has fired so the handles are freed before the loop is closed.
        uv_run(&loop, UV_RUN_DEFAULT);
        uv_loop_close(&loop);
    }
};

namespace
{
class EventLoopHelpers
{
public:
    static void boundingTimerNoop(uv_timer_t*) {}
};
} // namespace

#else

struct EventLoop::Poller
{
};

#endif

EventLoop::EventLoop(std::shared_ptr<WorkLedger> ledger)
    : ledger(std::move(ledger))
{
#if !defined(__EMSCRIPTEN__)
    poller = std::make_unique<Poller>();
#endif
}

EventLoop::~EventLoop() = default;

#if !defined(__EMSCRIPTEN__)
bool EventLoop::onLoopThread() const
{
    return std::this_thread::get_id() == loopThread.load(std::memory_order_acquire);
}
#endif

// Marks the jobs that run inside a poll, whose posts the host never needs to hear of, since the poll runs them itself.
EventLoop::PumpScope::PumpScope(EventLoop& loop)
    : loop(loop)
{
    loop.pumping = true;
}

EventLoop::PumpScope::~PumpScope()
{
    loop.pumping = false;
}

void EventLoop::wakeFromAnotherThread()
{
#if defined(__EMSCRIPTEN__)
    // The browser runs everything on one thread, so the post a host needs to hear of is one a callback of the page made while no poll runs, such as the end of a fetch.
    if (pumping)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(wakeMutex);
    if (wakeHandler)
    {
        wakeHandler();
    }
#else
    // Interrupt the wait only for a cross-thread post since the loop re-checks its job queue before every poll.
    if (poller && !onLoopThread())
    {
        poller->wakeAsync();

        // A host that drives the loop from its own run loop sleeps there, so it hears of the post as well.
        std::lock_guard<std::mutex> lock(wakeMutex);
        if (wakeHandler)
        {
            wakeHandler();
        }
    }
#endif
}

void EventLoop::post(Job job)
{
    ledger->enter();
    try
    {
        std::lock_guard<std::mutex> lock(mutex);
        // clang-format off
        jobs.push([ledger = ledger, j = std::move(job)]() mutable
        {
            // Release the ledger entry even if the job throws so the loop still drains.
            try
            {
                j();
            }
            catch (...)
            {
            }

            ledger->leave();
        });
        // clang-format on
    }
    catch (...)
    {
        // The job never entered the queue, so release the entry it will never run to balance.
        ledger->leave();
        throw;
    }

    wakeFromAnotherThread();
}

EventLoop::TimerId EventLoop::postDelayed(long long delayMs, Job job)
{
    // Schedule a timer that fires on the loop thread after the delay so it never occupies a worker thread while it waits.
    ledger->enter();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
    TimerId id = 0;
    try
    {
        std::lock_guard<std::mutex> lock(mutex);
        id = ++nextTimer;
        // clang-format off
        const auto placed = timers.emplace(deadline, Timer{id, [ledger = ledger, j = std::move(job)]() mutable
        {
            // Release the ledger entry even if the job throws so the loop still drains.
            try
            {
                j();
            }
            catch (...)
            {
            }

            ledger->leave();
        }});
        // clang-format on
        timerIndex.emplace(id, placed);
    }
    catch (...)
    {
        // The timer never entered the queue, so release the entry it will never run to balance.
        ledger->leave();
        throw;
    }

    wakeFromAnotherThread();
    return id;
}

// A cancelled timer never runs and gives back its ledger entry, so a deadline nobody waits for keeps the loop neither busy nor alive.
bool EventLoop::cancelTimer(TimerId id)
{
    Job dropped;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = timerIndex.find(id);
        if (found == timerIndex.end())
        {
            return false;
        }

        dropped = std::move(found->second->second.job);
        timers.erase(found->second);
        timerIndex.erase(found);
    }

    ledger->leave();
    return true;
}

#if !defined(__EMSCRIPTEN__)

void EventLoop::watchRead(const Poco::Net::Socket& socket, IoHandler handler)
{
    if (onLoopThread())
    {
        poller->addReader(socket, std::move(handler));
        return;
    }

    Poller* p = poller.get();
    p->submit([p, socket, handler = std::move(handler)]() mutable
              { p->addReader(socket, std::move(handler)); });
}

void EventLoop::watchWrite(const Poco::Net::Socket& socket, IoHandler handler)
{
    if (onLoopThread())
    {
        poller->addWriter(socket, std::move(handler));
        return;
    }

    Poller* p = poller.get();
    p->submit([p, socket, handler = std::move(handler)]() mutable
              { p->addWriter(socket, std::move(handler)); });
}

void EventLoop::closeSocket(const Poco::Net::Socket& socket)
{
    if (onLoopThread())
    {
        poller->closeNow(socket);
        return;
    }

    Poller* p = poller.get();
    p->submit([p, socket]() mutable
              { p->closeNow(socket); });
}

bool EventLoop::isRunning() const
{
    return running.load(std::memory_order_acquire);
}

void EventLoop::shutdownIo()
{
    poller->clear();
}

#endif

// Moves every timer whose deadline has passed into the ready queue, and is called with the mutex held.
void EventLoop::queueDueTimers()
{
    const auto now = std::chrono::steady_clock::now();
    while (!timers.empty() && timers.begin()->first <= now)
    {
        timerIndex.erase(timers.begin()->second.id);
        jobs.push(std::move(timers.begin()->second.job));
        timers.erase(timers.begin());
    }
}

// Runs the jobs queued and the timers due when the pass starts, so work they arm waits for the next pass, and stops early once the deadline passed after at least one job ran.
std::size_t EventLoop::runPass(Clock::time_point deadline)
{
    std::size_t batch = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        queueDueTimers();
        batch = jobs.size();
    }

    std::size_t ran = 0;
    while (ran < batch && running.load(std::memory_order_acquire))
    {
        if (ran > 0 && Clock::now() >= deadline)
        {
            break;
        }

        Job job;
        {
            // A stop from another thread may have dropped the queue since the pass counted it.
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

        ++ran;
    }

    return ran;
}

// Applies the socket operations other threads queued and serves every socket that is ready without waiting, answering how many operations and handlers ran.
std::size_t EventLoop::pollIo()
{
#if defined(__EMSCRIPTEN__)
    return 0;
#else
    const std::size_t applied = poller->drainCommands();
    poller->handlerRuns = 0;
    if (poller->hasHandles())
    {
        uv_run(&poller->loop, UV_RUN_NOWAIT);
    }

    return applied + poller->handlerRuns;
#endif
}

// Runs the entry the calling thread opened with `enter` until it is stopped or idle, so a stop that landed after the entry opened ends it at once.
void EventLoop::run()
{
#if defined(__EMSCRIPTEN__)
    // Wasm drives the loop by pumping `drainPostedJobs` from the host main loop so `run` is never entered there.
    return;
#else
    while (running.load(std::memory_order_acquire))
    {
        runPass(Clock::time_point::max());

        if (!running.load(std::memory_order_acquire))
        {
            break;
        }

        // Apply socket operations queued from other threads before deciding how long to wait.
        poller->drainCommands();

        long long timeoutMs = 1000;
        bool ready = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const bool haveTimers = !timers.empty();
            const bool haveSockets = poller->hasSockets();
            if (!jobs.empty())
            {
                ready = true;
            }
            else if (!haveTimers && !haveSockets && keepAlive == 0)
            {
                if (idleExitEligible && idleExitEligible())
                {
                    running.store(false, std::memory_order_release);
                    break;
                }
            }
            else if (haveTimers)
            {
                const auto now = std::chrono::steady_clock::now();
                const auto next = timers.begin()->first;
                const long long ms =
                    next <= now ? 0 : std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count();
                // Cap the wait so a missed wakeup self-heals within a tick.
                timeoutMs = std::min<long long>(ms, 1000);
            }
        }

        // While work is ready the sockets get their turn without a wait, so a pass that keeps posting work never starves them, and a loop with no socket polls nothing until it waits.
        if (ready || timeoutMs == 0)
        {
            if (poller->hasHandles())
            {
                uv_run(&poller->loop, UV_RUN_NOWAIT);
            }

            continue;
        }

        // Bound the libuv wait by the next timer deadline, though socket readiness or a cross-thread wakeup returns it sooner.
        uv_timer_start(&poller->timer, &EventLoopHelpers::boundingTimerNoop, static_cast<std::uint64_t>(timeoutMs), 0);
        uv_run(&poller->loop, UV_RUN_ONCE);
        uv_timer_stop(&poller->timer);
    }
#endif
}

// Makes the calling thread the loop thread and opens an entry, so work armed from Lua is applied directly rather than queued and a stop from then on ends the entry.
void EventLoop::enter()
{
    loopThread.store(std::this_thread::get_id(), std::memory_order_release);
    running.store(true);
}

// Advances the loop without ever blocking, running every pass of work there is and then one pass over the sockets, so a host that owns its own run loop drives the runtime from it.
bool EventLoop::poll()
{
    // An entry that finished asks the loop to stop, which under this model ends that entry rather than the runtime.
    enter();
    const PumpScope pump(*this);

    while (runPass(Clock::time_point::max()) > 0)
    {
    }

    if (running.load(std::memory_order_acquire))
    {
        pollIo();
    }

    return pending();
}

// Advances the loop without ever blocking, repeating passes of work and of the sockets while they make progress and the budget lasts.
// The budget is checked between jobs and between passes, so a poll overruns it by at most one job and one pass over the sockets, and always makes progress when work is ready.
EventLoop::PollResult EventLoop::poll(std::chrono::nanoseconds budget)
{
    enter();
    const PumpScope pump(*this);

    const auto started = Clock::now();
    const auto room = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::time_point::max() - started);
    const auto deadline = budget >= room ? Clock::time_point::max() : started + std::chrono::duration_cast<Clock::duration>(std::max(budget, std::chrono::nanoseconds::zero()));

    PollResult result;
    for (bool first = true; first || Clock::now() < deadline; first = false)
    {
        std::size_t progressed = runPass(deadline);
        if (!running.load(std::memory_order_acquire))
        {
            result.ran = result.ran || progressed > 0;
            break;
        }

        progressed += pollIo();
        result.ran = result.ran || progressed > 0;
        if (progressed == 0)
        {
            break;
        }
    }

    result.pending = pending();
    result.idleMilliseconds = idleMilliseconds();
    return result;
}

// Reports whether anything could still make progress, which is what tells a host whether to keep pumping.
bool EventLoop::pending() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!jobs.empty() || !timers.empty() || keepAlive > 0)
    {
        return true;
    }

#if defined(__EMSCRIPTEN__)
    return false;
#else
    return poller->hasSockets();
#endif
}

void EventLoop::stop()
{
    running.store(false, std::memory_order_release);
#if !defined(__EMSCRIPTEN__)
    if (poller)
    {
        poller->wakeAsync();
    }
#endif
}

void EventLoop::clearPendingJobs()
{
    // Drop queued jobs and timers without invoking them and release their ledger entries outside the lock so the notify path of `leave` stays unlocked.
    std::queue<Job> drainedJobs;
    Timers drainedTimers;
    {
        std::lock_guard<std::mutex> lock(mutex);
        jobs.swap(drainedJobs);
        timers.swap(drainedTimers);
        timerIndex.clear();
    }

    const std::size_t dropped = drainedJobs.size() + drainedTimers.size();
    for (std::size_t i = 0; i < dropped; ++i)
    {
        ledger->leave();
    }
}

// The count shares the mutex the exit decision is taken under, so a retain from another thread is never read late.
void EventLoop::retain()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++keepAlive;
    }

    wake();
}

// Releasing what was never retained would drive the count negative and leave the loop unable to ever exit.
bool EventLoop::release()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (keepAlive == 0)
        {
            return false;
        }

        --keepAlive;
    }

    wake();
    return true;
}

// Interrupts a wait of the loop, which only another thread needs, since the loop checks its state again before it waits.
void EventLoop::wake()
{
#if !defined(__EMSCRIPTEN__)
    if (poller && !onLoopThread())
    {
        poller->wakeAsync();
    }
#endif
}

void EventLoop::setIdleExitPredicate(IdleExitPredicate predicate)
{
    std::lock_guard<std::mutex> lock(mutex);
    idleExitEligible = std::move(predicate);
}

// Replaces the function a post from another thread calls, and returns only once no call of the previous one is still running.
void EventLoop::setWakeHandler(WakeHandler handler)
{
    std::lock_guard<std::mutex> lock(wakeMutex);
    wakeHandler = std::move(handler);
}

// Answers how long a host that polls can sleep before the loop has work of its own, and minus one when only a post can bring work.
long long EventLoop::idleMilliseconds() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!jobs.empty())
    {
        return 0;
    }

    long long idle = -1;
    if (!timers.empty())
    {
        const auto left = std::chrono::ceil<std::chrono::milliseconds>(timers.begin()->first - std::chrono::steady_clock::now());
        idle = std::max<long long>(0, left.count());
    }

#if !defined(__EMSCRIPTEN__)
    // A socket is only read when the loop is polled, so a host keeps polling while one is watched.
    constexpr long long socketPollMilliseconds = 50;
    if (poller->hasSockets())
    {
        idle = idle < 0 ? socketPollMilliseconds : std::min(idle, socketPollMilliseconds);
    }
#endif

    return idle;
}

bool EventLoop::hasPendingJobs() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!jobs.empty())
    {
        return true;
    }

    // Count a timer whose deadline has arrived as pending for any caller that only pumps via `drainPostedJobs`.
    return !timers.empty() && timers.begin()->first <= std::chrono::steady_clock::now();
}

bool EventLoop::hasPendingTimers() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return !timers.empty();
}

#if defined(__EMSCRIPTEN__)
void EventLoop::drainPostedJobs()
{
    const PumpScope pump(*this);
    for (;;)
    {
        Job job;
        {
            std::lock_guard<std::mutex> lock(mutex);
            // Move any timer whose deadline has passed into the ready queue so a wasm pump that never calls `run()` still fires `postDelayed` jobs.
            queueDueTimers();

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
#endif

} // namespace varn::runtime
