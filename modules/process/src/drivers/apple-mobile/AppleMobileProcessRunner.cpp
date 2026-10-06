#include "varn/process/ProcessRunner.h"

#include <stdexcept>
#include <string>

namespace varn::process
{

// The Apple systems besides macOS let an app read its environment and its working directory, which the POSIX environment of this driver reads, but never let it start another process.
bool ProcessRunner::available()
{
    return false;
}

ProcessResult ProcessRunner::exec(const std::string& /*command*/, long long /*timeoutMs*/)
{
    throw std::runtime_error("This system does not let an app start another process.");
}

} // namespace varn::process
