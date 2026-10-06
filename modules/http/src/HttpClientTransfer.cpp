#include "HttpClientTransfer.h"

#include "HttpClientFailure.h"
#include "varn/log/Log.h"
#include "varn/runtime/Runtime.h"

#include <exception>
#include <utility>

namespace varn::http::client
{

namespace
{
// The body a transport may have handed to the loop that the loop has not delivered yet, which bounds what a slow consumer costs in memory.
constexpr std::size_t kWindowBytes = 4u * 1024u * 1024u;

// A paused transport resumes once the loop has delivered half of what it holds, so it neither stalls nor flaps around the bound.
constexpr std::size_t kResumeBytes = kWindowBytes / 2;

// A transport waiting for room checks this often whether the runtime stopped or the deadline passed, since neither wakes it.
constexpr std::chrono::milliseconds kRoomCheck{50};
} // namespace

HttpClientTransfer::HttpClientTransfer(varn::runtime::Runtime& runtime, Request request, HttpClient::Callbacks callbacks)
    : runtime(runtime)
    , wanted(std::move(request))
    , callbacks(std::move(callbacks))
{
}

// Arms the one deadline of the whole request on the loop, so it ends the transfer on time whatever its transport waits for.
void HttpClientTransfer::begin()
{
    end = std::chrono::steady_clock::now() + wanted.timeout;
    std::weak_ptr<HttpClientTransfer> weak = weak_from_this();

    // clang-format off
    timer = runtime.mainLoop().postDelayed(static_cast<long long>(wanted.timeout.count()), [weak]
    {
        if (const auto self = weak.lock())
        {
            self->halt(self->timeoutError(), true);
        }
    });
    // clang-format on
}

void HttpClientTransfer::cancel()
{
    halt(Error{ErrorCode::Cancelled, "[HttpClient] The request was cancelled."}, true);
}

// Ends the transfer without calling any of its callbacks again, for a client that goes away while it runs.
// A completion or a chunk already on its way to the loop is dropped too, whichever end claimed the transfer first.
void HttpClientTransfer::abandon()
{
    abandoned.store(true, std::memory_order_release);
    halt(Error{ErrorCode::Cancelled, "[HttpClient] The request was cancelled."}, false);
}

Error HttpClientTransfer::timeoutError() const
{
    return Error{ErrorCode::Timeout, "[HttpClient] The request to \"" + wanted.url + "\" did not finish within its timeout."};
}

Error HttpClientTransfer::tooLargeError() const
{
    return Error{ErrorCode::TooLarge, "[HttpClient] The response of \"" + wanted.url + "\" is larger than the limit of " + std::to_string(wanted.maxResponseBytes) + " bytes of \"maxResponseBytes\"."};
}

// Registers what stops the transport where it waits, and refuses it once the transfer is already halted so the transport ends at once.
bool HttpClientTransfer::setInterrupt(std::function<void()> interrupt)
{
    std::lock_guard<std::mutex> lock(interruptMutex);
    if (halted())
    {
        return false;
    }

    interrupter = std::move(interrupt);
    return true;
}

void HttpClientTransfer::clearInterrupt()
{
    std::lock_guard<std::mutex> lock(interruptMutex);
    interrupter = nullptr;
}

// Lets a transport that cannot wait on its own thread pause while the loop holds a full window and resume once it drains.
void HttpClientTransfer::setFlow(std::function<void()> pause, std::function<void()> resume)
{
    std::lock_guard<std::mutex> lock(flowMutex);
    pauseFlow = std::move(pause);
    resumeFlow = std::move(resume);
}

void HttpClientTransfer::clearFlow()
{
    std::lock_guard<std::mutex> lock(flowMutex);
    pauseFlow = nullptr;
    resumeFlow = nullptr;
    paused = false;
}

// Hands the head of the final response to the loop, and refuses a body that declares itself larger than the limit before a byte of it is read.
bool HttpClientTransfer::head(ResponseHead head)
{
    if (halted())
    {
        return false;
    }

    if (head.contentLength && *head.contentLength > wanted.maxResponseBytes)
    {
        halt(tooLargeError(), true);
        return false;
    }

    total = head.contentLength;
    auto self = shared_from_this();

    // clang-format off
    runtime.mainLoop().post([self, head = std::move(head)]
    {
        if (self->skipping())
        {
            return;
        }

        self->invoke([&]
        {
            if (self->callbacks.onHead)
            {
                self->callbacks.onHead(head);
            }
        });
    });
    // clang-format on
    return true;
}

// Hands one piece of the body to the loop, and answers whether the transport should keep reading.
// A transport without a pause waits here while the loop holds a full window, which is what keeps a slow consumer from buffering the whole body.
bool HttpClientTransfer::deliver(const char* data, std::size_t length)
{
    if (halted())
    {
        return false;
    }

    if (length == 0)
    {
        return true;
    }

    received += length;
    if (received > wanted.maxResponseBytes)
    {
        halt(tooLargeError(), true);
        return false;
    }

    std::string chunk(data, length);
    const Progress progress{received, total};
    bool waits = false;
    {
        std::lock_guard<std::mutex> lock(flowMutex);
        inflight += length;
        waits = !pauseFlow;

        if (pauseFlow && !paused && inflight >= kWindowBytes)
        {
            paused = true;
            pauseFlow();
        }
    }

    auto self = shared_from_this();

    // clang-format off
    runtime.mainLoop().post([self, chunk = std::move(chunk), progress]
    {
        self->release(chunk.size());
        if (self->skipping())
        {
            return;
        }

        self->invoke([&]
        {
            if (self->callbacks.onChunk)
            {
                self->callbacks.onChunk(chunk);
            }
        });

        if (self->skipping())
        {
            return;
        }

        self->invoke([&]
        {
            if (self->callbacks.onProgress)
            {
                self->callbacks.onProgress(progress);
            }
        });
    });
    // clang-format on

    if (waits)
    {
        awaitRoom();
    }

    return !halted();
}

// Reads the next piece of a body that streams from the caller, refusing one that writes more or less than the length it declared.
std::size_t HttpClientTransfer::readBody(char* buffer, std::size_t capacity)
{
    if (halted())
    {
        throw HttpClientFailure(ErrorCode::Cancelled, "[HttpClient] The request was cancelled.");
    }

    std::size_t written = 0;
    try
    {
        written = wanted.bodySource(buffer, capacity);
    }
    catch (const std::exception& failure)
    {
        throw HttpClientFailure(ErrorCode::Callback, std::string("[HttpClient] The body source failed: ") + failure.what());
    }
    catch (...)
    {
        throw HttpClientFailure(ErrorCode::Callback, "[HttpClient] The body source failed without a message.");
    }

    if (written > capacity)
    {
        throw HttpClientFailure(ErrorCode::Callback, "[HttpClient] The body source answered more bytes than its buffer holds.");
    }

    sent += written;
    const bool overran = wanted.bodyLength && sent > *wanted.bodyLength;
    const bool shortened = wanted.bodyLength && written == 0 && sent < *wanted.bodyLength;
    if (overran || shortened)
    {
        throw HttpClientFailure(ErrorCode::Invalid, "[HttpClient] The body source wrote " + std::string(overran ? "more" : "less") + " than the " + std::to_string(*wanted.bodyLength) + " bytes of \"bodyLength\".");
    }

    return written;
}

void HttpClientTransfer::succeed()
{
    settle(std::nullopt);
}

void HttpClientTransfer::fail(Error error)
{
    settle(std::move(error));
}

// Ends a transfer its transport finished, behind the head and the chunks it already posted, unless something else ended it first.
void HttpClientTransfer::settle(std::optional<Error> error)
{
    if (claimed.exchange(true, std::memory_order_acq_rel))
    {
        return;
    }

    auto self = shared_from_this();

    // clang-format off
    runtime.mainLoop().post([self, error = std::move(error)]
    {
        self->complete(error, true);
    });
    // clang-format on
}

// Ends the transfer from outside its transport, stopping the transport where it waits and dropping whatever it already handed to the loop.
void HttpClientTransfer::halt(Error error, bool notify)
{
    if (claimed.exchange(true, std::memory_order_acq_rel))
    {
        return;
    }

    stopping.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(interruptMutex);
        if (interrupter)
        {
            interrupter();
        }
    }

    {
        std::lock_guard<std::mutex> lock(flowMutex);
    }
    room.notify_all();

    auto self = shared_from_this();

    // clang-format off
    runtime.mainLoop().post([self, error = std::move(error), notify]
    {
        self->complete(error, notify);
    });
    // clang-format on
}

// Calls the completion once on the loop and releases every callback there, since one may hold state only the loop may touch.
void HttpClientTransfer::complete(const std::optional<Error>& error, bool notify)
{
    finished = true;
    runtime.mainLoop().cancelTimer(timer);
    const HttpClient::Callbacks done = std::move(callbacks);
    callbacks = HttpClient::Callbacks{};

    if (!notify || !done.onComplete || abandoned.load(std::memory_order_acquire) || runtime.stopped())
    {
        return;
    }

    try
    {
        done.onComplete(error);
    }
    catch (const std::exception& failure)
    {
        varn::log::Log::error("HttpClient", std::string("The completion callback failed: ") + failure.what());
    }
    catch (...)
    {
        varn::log::Log::error("HttpClient", "The completion callback failed without a message.");
    }
}

// Waits on the thread of the transport until the loop has room for more of the body, the transfer halts, the deadline passes or the runtime stops.
void HttpClientTransfer::awaitRoom()
{
    std::unique_lock<std::mutex> lock(flowMutex);
    while (inflight >= kWindowBytes && !halted())
    {
        if (runtime.stopped())
        {
            lock.unlock();
            abandon();
            return;
        }

        if (std::chrono::steady_clock::now() >= end)
        {
            lock.unlock();
            halt(timeoutError(), true);
            return;
        }

        room.wait_for(lock, kRoomCheck);
    }
}

void HttpClientTransfer::release(std::size_t length)
{
    {
        std::lock_guard<std::mutex> lock(flowMutex);
        inflight -= length;

        if (paused && inflight <= kResumeBytes)
        {
            paused = false;
            if (resumeFlow)
            {
                resumeFlow();
            }
        }
    }

    room.notify_all();
}

// A job of the loop delivers nothing once the transfer ended or the runtime stopped.
bool HttpClientTransfer::skipping() const
{
    return finished || halted() || abandoned.load(std::memory_order_acquire) || runtime.stopped();
}

// Runs a callback of the caller, and ends the transfer with its failure, since a callback that throws cannot be told to stop any other way.
template <typename Work>
void HttpClientTransfer::invoke(const Work& work)
{
    try
    {
        work();
    }
    catch (const std::exception& failure)
    {
        halt(Error{ErrorCode::Callback, std::string("[HttpClient] A callback failed: ") + failure.what()}, true);
    }
    catch (...)
    {
        halt(Error{ErrorCode::Callback, "[HttpClient] A callback failed without a message."}, true);
    }
}

} // namespace varn::http::client
