#include "varn/process/ProcessRunner.h"

#include <cerrno>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cstdlib>
#include <unistd.h>

extern char** environ;

namespace varn::process
{

std::optional<std::string> ProcessRunner::getenv(const std::string& name)
{
    const char* value = ::getenv(name.c_str());
    if (value == nullptr)
    {
        return std::nullopt;
    }

    return std::string(value);
}

std::vector<std::pair<std::string, std::string>> ProcessRunner::environment()
{
    std::vector<std::pair<std::string, std::string>> entries;

    for (char** env = ::environ; env != nullptr && *env != nullptr; ++env)
    {
        const std::string line = *env;
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }

        entries.emplace_back(line.substr(0, equals), line.substr(equals + 1));
    }

    return entries;
}

std::string ProcessRunner::cwd()
{
    std::vector<char> buffer(4096);

    while (true)
    {
        if (::getcwd(buffer.data(), buffer.size()) != nullptr)
        {
            return std::string(buffer.data());
        }

        if (errno != ERANGE)
        {
            throw std::runtime_error("The current working directory could not be read.");
        }

        buffer.resize(buffer.size() * 2);
    }
}

} // namespace varn::process
