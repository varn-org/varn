#include "HttpRange.h"

#include <charconv>
#include <limits>

namespace varn::http
{

// Reads a position of a range, where a number past the width of the count is still a position past every file rather than an error.
std::optional<std::uintmax_t> HttpRange::readPosition(const std::string& digits)
{
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
    {
        return std::nullopt;
    }

    std::uintmax_t value = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (error == std::errc::result_out_of_range)
    {
        return std::numeric_limits<std::uintmax_t>::max();
    }

    return value;
}

// Reads the one `bytes` range of RFC 9110 the server serves, and answers nothing for any other header, which the server then ignores.
std::optional<HttpRange::Spec> HttpRange::read(const std::string& header)
{
    const std::string prefix = "bytes=";
    if (header.rfind(prefix, 0) != 0)
    {
        return std::nullopt;
    }

    const std::string spec = header.substr(prefix.size());
    const std::size_t dash = spec.find('-');
    if (dash == std::string::npos)
    {
        return std::nullopt;
    }

    const std::string first = spec.substr(0, dash);
    const std::string last = spec.substr(dash + 1);
    Spec range;
    if (!first.empty())
    {
        range.first = readPosition(first);
        if (!range.first)
        {
            return std::nullopt;
        }
    }

    if (!last.empty())
    {
        range.last = readPosition(last);
        if (!range.last)
        {
            return std::nullopt;
        }
    }

    // A range names at least one end, and one whose last position comes before its first is not a range at all.
    if ((!range.first && !range.last) || (range.first && range.last && *range.last < *range.first))
    {
        return std::nullopt;
    }

    return range;
}

bool HttpRange::isByteRange(const std::string& header)
{
    return read(header).has_value();
}

// Resolves the range against the length of the file and answers whether any of the file lies in it, which a range of a file without bytes never does.
bool HttpRange::parse(const std::string& header, std::uintmax_t total, std::uintmax_t& start, std::uintmax_t& end)
{
    const std::optional<Spec> range = read(header);
    if (!range || total == 0)
    {
        return false;
    }

    // A suffix range asks for the final bytes of the file.
    if (!range->first)
    {
        if (*range->last == 0)
        {
            return false;
        }

        start = *range->last >= total ? 0 : total - *range->last;
        end = total - 1;
        return true;
    }

    if (*range->first >= total)
    {
        return false;
    }

    start = *range->first;
    end = range->last && *range->last < total ? *range->last : total - 1;
    return true;
}

} // namespace varn::http
