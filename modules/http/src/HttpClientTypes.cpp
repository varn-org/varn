#include "varn/http/HttpClientTypes.h"

#include <cctype>
#include <charconv>

namespace varn::http::client
{

namespace
{
class HeadText
{
public:
    HeadText() = delete;

    static bool sameName(std::string_view first, std::string_view second)
    {
        if (first.size() != second.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < first.size(); ++index)
        {
            if (std::tolower(static_cast<unsigned char>(first[index])) != std::tolower(static_cast<unsigned char>(second[index])))
            {
                return false;
            }
        }

        return true;
    }

    // Reads a whole decimal number, refusing anything around it or past what the type holds.
    static std::optional<std::uint64_t> number(std::string_view text)
    {
        std::uint64_t value = 0;
        const char* last = text.data() + text.size();
        const auto [stop, failure] = std::from_chars(text.data(), last, value);
        if (text.empty() || failure != std::errc() || stop != last)
        {
            return std::nullopt;
        }

        return value;
    }
};
} // namespace

// Answers the first value of a header, whatever the case of its name.
std::optional<std::string_view> ResponseHead::header(std::string_view name) const
{
    for (const auto& [key, value] : headers)
    {
        if (HeadText::sameName(key, name))
        {
            return std::string_view(value);
        }
    }

    return std::nullopt;
}

// Answers the range a partial response carries, from a "Content-Range" of the form "bytes first-last/length" or "bytes first-last/*".
std::optional<ContentRange> ResponseHead::contentRange() const
{
    const std::optional<std::string_view> found = header("content-range");
    constexpr std::string_view unit = "bytes ";
    if (!found || found->substr(0, unit.size()) != unit)
    {
        return std::nullopt;
    }

    const std::string_view spec = found->substr(unit.size());
    const std::size_t dash = spec.find('-');
    const std::size_t slash = spec.find('/');
    if (dash == std::string_view::npos || slash == std::string_view::npos || dash > slash)
    {
        return std::nullopt;
    }

    const std::optional<std::uint64_t> first = HeadText::number(spec.substr(0, dash));
    const std::optional<std::uint64_t> last = HeadText::number(spec.substr(dash + 1, slash - dash - 1));
    if (!first || !last || *last < *first)
    {
        return std::nullopt;
    }

    ContentRange range{*first, *last, std::nullopt};
    const std::string_view length = spec.substr(slash + 1);
    if (length == "*")
    {
        return range;
    }

    range.completeLength = HeadText::number(length);
    if (!range.completeLength || *range.completeLength <= *last)
    {
        return std::nullopt;
    }

    return range;
}

} // namespace varn::http::client
