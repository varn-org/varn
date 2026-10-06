#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace varn::http
{

class HttpRange
{
public:
    HttpRange() = delete;

    static bool isByteRange(const std::string& header);
    static bool parse(const std::string& header, std::uintmax_t total, std::uintmax_t& start, std::uintmax_t& end);

private:
    struct Spec
    {
        std::optional<std::uintmax_t> first;
        std::optional<std::uintmax_t> last;
    };

    static std::optional<Spec> read(const std::string& header);
    static std::optional<std::uintmax_t> readPosition(const std::string& digits);
};

} // namespace varn::http
