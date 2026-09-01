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

    SocketState* ensure(const Poco::Net::Socket& socket)
    {
        auto it = entries.find(socket);
        if (it != entries.end())
        {
            return it->second;
        }

        auto* state = new SocketState{};
        state->socket = socket;
        state->owner = this;
        state->handle.data = state;
        uv_poll_init_socket(&loop, &state->handle, socket.impl()->sockfd());
        entries.emplace(socket, state);
        return state;
    }

    void addReader(const Poco::Net::Socket& socket, IoHandler handler)
    {
        SocketState* state = ensure(socket);
        state->readers.push_back(std::move(handler));
        refreshMode(state);
    }

    void addWriter(const Poco::Net::Socket& socket, IoHandler handler)
    {
        SocketState* state = ensure(socket);
        state->writers.push_back(std::move(handler));
        refreshMode(state);
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

        // mark the state retiring so it outlives this call until libuv frees the handle in its close callback
        state->retiring = true;
        entries.erase(state->socket);
        uv_poll_stop(&state->handle);
        uv_close(reinterpret_cast<uv_handle_t*>(&state->handle),
                 [](uv_handle_t* handle)
                 { delete static_cast<SocketState*>(handle->data); });
    }

    void serve(SocketState* state, int status, int events)
    {
        const bool errored = status < 0;

        if ((events & UV_READABLE) || errored)
        {
            if (!state->readers.empty())
            {
                // pop the handler before invoking it so a close triggered inside the handler cannot rerun it
                IoHandler handler = std::move(state->readers.front());
                state->readers.pop_front();
                bool done = true;
                try
                {
                    done = handler();
                }
                catch (...)
                {
                }

                if (state->retiring)
                {
                    return;
                }

                if (!done)
                {
                    state->readers.push_front(std::move(handler));
                }
            }
        }

        if (state->retiring)
        {
            return;
        }

        if ((events & UV_WRITABLE) || errored)
        {
            if (!state->writers.empty())
            {
                IoHandler handler = std::move(state->writers.front());
                state->writers.pop_front();
                bool done = true;
                try
                {
                    done = handler();
                }
                catch (...)
                {
                }

                if (state->retiring)
                {
                    return;
                }

                if (!done)
                {
                    state->writers.push_front(std::move(handler));
                }
            }
        }

        if (!state->retiring)
        {
            refreshMode(state);
        }
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

        // close the fd so each pending handler's next i/o call fails, then retire the handle and run them so promises reject
        try
        {
            state->socket.impl()->close();
        }
        catch (...)
        {
        }

        retire(state);
        for (auto& reader : readers)
        {
            try
            {
                reader();
            }
            catch (...)
            {
            }
        }

        for (auto& writer : writers)
        {
            try
            {
                writer();
            }
            catch (...)
            {
            }
        }
    }

    void wakeAsync()
    {
        // serialize the wakeup with clear so a cross-thread send never races the handle being closed
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

    void drainCommands()
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
    }

    bool hasSockets() const
    {
        return !entries.empty();
    }

    void clear()
    {
        {
            // mark closed under the same mutex the wakeups use so no cross-thread send can target the async handle after this
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
            uv_close(reinterpret_cast<uv_handle_t*>(&state->handle),
                     [](uv_handle_t* handle)
                     { delete static_cast<SocketState*>(handle->data); });
        }

        uv_close(reinterpret_cast<uv_handle_t*>(&async), nullptr);
        uv_close(reinterpret_cast<uv_handle_t*>(&timer), nullptr);
        // run the loop until every close callback has fired so the handles are freed before the loop is closed
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

void EventLoop::wakeFromAnotherThread()
{
#if !defined(__EMSCRIPTEN__)
    // interrupt the wait only for a cross-thread post since the loop re-checks its job queue before every poll
    if (poller && !onLoopThread())
    {
        poller->wakeAsync();
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
            // release the ledger entry even if the job throws so the loop still drains
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
        // the job never entered the queue, so release the entry it will never run to balance
        ledger->leave();
        throw;
    }

    wakeFromAnotherThread();
}

void EventLoop::postDelayed(long long delayMs, Job job)
{
    // schedule a timer that fires on the loop thread after the delay so it never occupies a worker thread while it waits
    ledger->enter();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
    try
    {
        std::lock_guard<std::mutex> lock(mutex);
        // clang-format off
        timers.emplace(deadline, [ledger = ledger, j = std::move(job)]() mutable
        {
            // release the ledger entry even if the job throws so the loop still drains
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
        // the timer never entered the queue, so release the entry it will never run to balance
        ledger->leave();
        throw;
    }

    wakeFromAnotherThread();
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

void EventLoop::run()
{
#if defined(__EMSCRIPTEN__)
    // wasm drives the loop by pumping drainPostedJobs from the host main loop so run is never entered there
    return;
#else
    running.store(true, std::memory_order_release);
    loopThread.store(std::this_thread::get_id(), std::memory_order_release);

    while (running.load(std::memory_order_acquire))
    {
        // run every ready job and due timer, any of which may post more work, arm sockets or stop the loop
        for (;;)
        {
            if (!running.load(std::memory_order_acquire))
            {
                break;
            }

            Job job;
            {
                std::lock_guard<std::mutex> lock(mutex);
                const auto now = std::chrono::steady_clock::now();
                while (!timers.empty() && timers.begin()->first <= now)
                {
                    jobs.push(std::move(timers.begin()->second));
                    timers.erase(timers.begin());
                }

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

        if (!running.load(std::memory_order_acquire))
        {
            break;
        }

        // apply socket operations queued from other threads before deciding how long to wait
        poller->drainCommands();

        long long timeoutMs = 1000;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!jobs.empty())
            {
                continue;
            }

            const bool haveTimers = !timers.empty();
            const bool haveSockets = poller->hasSockets();
            if (!haveTimers && !haveSockets)
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
                // cap the wait so a missed wakeup self-heals within a tick
                timeoutMs = std::min<long long>(ms, 1000);
            }
        }

        // bound the libuv wait by the next timer deadline, though socket readiness or a cross-thread wakeup returns it sooner
        uv_timer_start(&poller->timer, &EventLoopHelpers::boundingTimerNoop, static_cast<std::uint64_t>(timeoutMs), 0);
        uv_run(&poller->loop, UV_RUN_ONCE);
        uv_timer_stop(&poller->timer);
    }

    // clear the io state on the loop thread that owns it once the loop has exited
    shutdownIo();
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
    // drop queued jobs and timers without invoking them and release their ledger entries outside the lock so leave's notify path stays unlocked
    std::queue<Job> drainedJobs;
    std::multimap<std::chrono::steady_clock::time_point, Job> drainedTimers;
    {
        std::lock_guard<std::mutex> lock(mutex);
        jobs.swap(drainedJobs);
        timers.swap(drainedTimers);
    }

    const std::size_t dropped = drainedJobs.size() + drainedTimers.size();
    for (std::size_t i = 0; i < dropped; ++i)
    {
        ledger->leave();
    }
}

void EventLoop::wake()
{
#if !defined(__EMSCRIPTEN__)
    if (poller)
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

bool EventLoop::hasPendingJobs() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!jobs.empty())
    {
        return true;
    }

    // count a timer whose deadline has arrived as pending for any caller that only pumps via drainPostedJobs
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
    for (;;)
    {
        Job job;
        {
            std::lock_guard<std::mutex> lock(mutex);
            // move any timer whose deadline has passed into the ready queue so a wasm pump that never calls run() still fires postDelayed jobs
            const auto now = std::chrono::steady_clock::now();
            while (!timers.empty() && timers.begin()->first <= now)
            {
                jobs.push(std::move(timers.begin()->second));
                timers.erase(timers.begin());
            }

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
