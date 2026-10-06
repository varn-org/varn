#include "varn/async/AsyncTask.h"

#include "varn/runtime/Runtime.h"
#include "varn/runtime/TaskPool.h"

#include <exception>
#include <memory>
#include <utility>

namespace varn::async
{

int AsyncTask::runOnPool(lua_State* L, varn::runtime::Runtime& runtime, varn::runtime::TaskPool& pool, std::string moduleTag, std::function<void(Promise&)> work)
{
    auto promise = std::make_shared<Promise>(runtime);

    // clang-format off
    pool.post([promise, moduleTag = std::move(moduleTag), work = std::move(work)]
    {
        try
        {
            work(*promise);
        }
        catch (const std::exception& ex)
        {
            promise->reject(ex.what());
        }
        catch (...)
        {
            promise->reject("[" + moduleTag + "] The operation failed with a non-standard error.");
        }
    });
    // clang-format on

    Promise::push(L, promise);
    return 1;
}

} // namespace varn::async
