#include "HttpText.h"

#include <cctype>
#include <string>

namespace varn::http
{

std::string HttpText::toLower(std::string value)
{
    for (char& c : value)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return value;
}

bool HttpText::iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }

    return true;
}

} // namespace varn::http
