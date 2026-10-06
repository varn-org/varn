#include "varn/process/ProcessRunner.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace varn::process
{

bool ProcessRunner::available()
{
    return false;
}

ProcessResult ProcessRunner::exec(const std::string& /*command*/, long long /*timeoutMs*/)
{
    throw std::runtime_error("The process module is not available in this build.");
}

std::optional<std::string> ProcessRunner::getenv(const std::string& /*name*/)
{
    throw std::runtime_error("The environment of the process is not available in this build.");
}

std::vector<std::pair<std::string, std::string>> ProcessRunner::environment()
{
    throw std::runtime_error("The environment of the process is not available in this build.");
}

std::string ProcessRunner::cwd()
{
    throw std::runtime_error("The working directory is not available in this build.");
}

} // namespace varn::process
