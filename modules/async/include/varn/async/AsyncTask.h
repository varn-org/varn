#pragma once

#include "varn/async/Promise.h"

#include <functional>
#include <string>

#include <lua.hpp>

namespace varn::runtime
{
class Runtime;
class TaskPool;
} // namespace varn::runtime

namespace varn::async
{

class AsyncTask
{
public:
    AsyncTask() = delete;

    static int runOnPool(lua_State* L, varn::runtime::Runtime& runtime, varn::runtime::TaskPool& pool, std::string moduleTag, std::function<void(Promise&)> work);
};

} // namespace varn::async
