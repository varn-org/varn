#pragma once

#include <cstdint>
#include <string>

namespace varn::http
{

class HttpRange
{
public:
    HttpRange() = delete;

    static bool parse(const std::string& header, std::uintmax_t total, std::uintmax_t& start, std::uintmax_t& end);

private:
    static bool allDigits(const std::string& value);
};

} // namespace varn::http
