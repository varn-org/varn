#pragma once

#include <string>
#include <string_view>

namespace varn::http
{

class HttpUrl
{
public:
    HttpUrl() = delete;

    static std::string encode(std::string_view input);
    static std::string decode(std::string_view input);
};

} // namespace varn::http
