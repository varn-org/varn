#include "HttpRange.h"

#include <exception>

namespace varn::http
{

bool HttpRange::allDigits(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    for (const char c : value)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
    }

    return true;
}

bool HttpRange::parse(const std::string& header, std::uintmax_t total, std::uintmax_t& start, std::uintmax_t& end)
{
    const std::string prefix = "bytes=";
    if (header.rfind(prefix, 0) != 0 || total == 0)
    {
        return false;
    }

    const std::string spec = header.substr(prefix.size());
    const std::size_t dash = spec.find('-');
    if (dash == std::string::npos)
    {
        return false;
    }

    const std::string first = spec.substr(0, dash);
    const std::string last = spec.substr(dash + 1);

    // each present endpoint must be a run of digits, and an empty endpoint is allowed on one side only
    if ((!first.empty() && !allDigits(first)) || (!last.empty() && !allDigits(last)))
    {
        return false;
    }

    try
    {
        if (first.empty())
        {
            // a suffix range asks for the final bytes of the file
            if (last.empty())
            {
                return false;
            }

            const std::uintmax_t count = std::stoull(last);
            if (count == 0)
            {
                return false;
            }

            start = count >= total ? 0 : total - count;
            end = total - 1;
            return true;
        }

        start = std::stoull(first);
        end = last.empty() ? total - 1 : std::stoull(last);
    }
    catch (const std::exception&)
    {
        return false;
    }

    if (end >= total)
    {
        end = total - 1;
    }

    return start <= end && start < total;
}

} // namespace varn::http
