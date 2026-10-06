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
    throw std::runtime_error("[ProcessRunner] The process module is not available in this build.");
}

std::optional<std::string> ProcessRunner::getenv(const std::string& /*name*/)
{
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> ProcessRunner::environment()
{
    return {};
}

std::string ProcessRunner::cwd()
{
    return "/";
}

} // namespace varn::process
