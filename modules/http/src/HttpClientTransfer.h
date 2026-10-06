#pragma once

#include "varn/http/HttpClient.h"
#include "varn/http/HttpClientTypes.h"
#include "varn/runtime/EventLoop.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace varn::runtime
{
class Runtime;
}

namespace varn::http::client
{

/// One request in flight, shared by the handle of its caller, the transport that runs it and the jobs that deliver it on the loop.
class HttpClientTransfer : public std::enable_shared_from_this<HttpClientTransfer>
{
public:
    HttpClientTransfer(varn::runtime::Runtime& runtime, Request request, HttpClient::Callbacks callbacks);

    HttpClientTransfer(const HttpClientTransfer&) = delete;
    HttpClientTransfer& operator=(const HttpClientTransfer&) = delete;

    const Request& request() const { return wanted; }
    std::chrono::steady_clock::time_point deadline() const { return end; }
    bool halted() const { return stopping.load(std::memory_order_acquire); }
    bool settled() const { return claimed.load(std::memory_order_acquire); }

    void begin();
    void cancel();
    void abandon();

    [[nodiscard]] bool setInterrupt(std::function<void()> interrupt);
    void clearInterrupt();
    void setFlow(std::function<void()> pause, std::function<void()> resume);
    void clearFlow();

    [[nodiscard]] bool head(ResponseHead head);
    [[nodiscard]] bool deliver(const char* data, std::size_t length);
    std::size_t readBody(char* buffer, std::size_t capacity);
    void succeed();
    void fail(Error error);
    Error timeoutError() const;

private:
    Error tooLargeError() const;
    void halt(Error error, bool notify);
    void settle(std::optional<Error> error);
    void complete(const std::optional<Error>& error, bool notify);
    void awaitRoom();
    void release(std::size_t length);
    bool skipping() const;
    template <typename Work>
    void invoke(const Work& work);

    varn::runtime::Runtime& runtime;
    Request wanted;
    HttpClient::Callbacks callbacks;
    std::chrono::steady_clock::time_point end;
    varn::runtime::EventLoop::TimerId timer = 0;
    std::atomic<bool> stopping{false};
    std::atomic<bool> claimed{false};
    std::atomic<bool> abandoned{false};
    bool finished = false;

    std::mutex interruptMutex;
    std::function<void()> interrupter;

    std::mutex flowMutex;
    std::condition_variable room;
    std::size_t inflight = 0;
    bool paused = false;
    std::function<void()> pauseFlow;
    std::function<void()> resumeFlow;

    std::uint64_t received = 0;
    std::uint64_t sent = 0;
    std::optional<std::uint64_t> total;
};

} // namespace varn::http::client
