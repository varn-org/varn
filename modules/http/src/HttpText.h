#pragma once

#include <string>

namespace varn::http
{

class HttpText
{
public:
    static std::string toLower(std::string value);
    static bool iequals(const std::string& a, const std::string& b);
    static bool validUtf8(const std::string& text);
};

} // namespace varn::http
