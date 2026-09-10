#pragma once

#include <functional>
#include <string_view>

namespace varn::runtime
{

class App
{
public:
    int run(int argc, char** argv);

    static int workerCount();

private:
    static bool isEvalFlag(std::string_view flag);
    static bool isVersionFlag(std::string_view flag);
    static bool isHelpFlag(std::string_view flag);
    static void printUsage();
    static bool isWorkerChild();
#if !defined(VARN_NO_FORK)
    static int superviseWorkers(int count, const std::function<int()>& runChild);
#endif
};

} // namespace varn::runtime
