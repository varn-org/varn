#pragma once

#include <atomic>

namespace varn::wasm
{

class WasmAsyncHost
{
public:
    WasmAsyncHost() = delete;

    static std::atomic<int>& fetchInflight()
    {
        static std::atomic<int> counter{0};
        return counter;
    }
};

} // namespace varn::wasm
