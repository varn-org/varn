#pragma once

#include <atomic>

namespace varn::runtime
{

class HostWork
{
public:
    HostWork() = delete;

    static void begin() { inflight().fetch_add(1, std::memory_order_relaxed); }
    static void end() { inflight().fetch_sub(1, std::memory_order_acq_rel); }
    static bool pending() { return inflight().load(std::memory_order_acquire) > 0; }

private:
    static std::atomic<int>& inflight()
    {
        static std::atomic<int> counter{0};
        return counter;
    }
};

} // namespace varn::runtime
