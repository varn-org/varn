#pragma once

#include <atomic>
#include <functional>

namespace varn::runtime
{

class WorkLedger
{
public:
    void setNotify(std::function<void()> fn) { notify = std::move(fn); }

    void enter() { count.fetch_add(1, std::memory_order_relaxed); }

    void leave()
    {
        if (count.fetch_sub(1, std::memory_order_acq_rel) == 1 && notify)
        {
            notify();
        }
    }

    int depth() const { return count.load(std::memory_order_acquire); }

private:
    std::atomic<int> count{0};
    std::function<void()> notify;
};

} // namespace varn::runtime
